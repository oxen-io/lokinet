#include "dnsd.hpp"
#include <string>
#include "ev.hpp"
#include "logger.hpp"
#include "net.hpp"

mtx_t m_dnsd_Mutex;
mtx_t m_dnsd2_Mutex;
mtx_t m_dnsd3_Mutex;

int
get16bits(const char *&buffer) throw()
{
  int value = static_cast< unsigned char >(buffer[0]);
  value     = value << 8;
  value += static_cast< unsigned char >(buffer[1]);
  buffer += 2;
  return value;
}

// uint32_t
uint32_t
get32bits(const char *&buffer) throw()
{
  uint32_t value = uint32_t(
      (unsigned char)(buffer[0]) << 24 | (unsigned char)(buffer[1]) << 16
      | (unsigned char)(buffer[2]) << 8 | (unsigned char)(buffer[3]));
  buffer += 4;
  return value;
}

void
put16bits(char *&buffer, uint value) throw()
{
  buffer[0] = (value & 0xFF00) >> 8;
  buffer[1] = value & 0xFF;
  buffer += 2;
}

void
put32bits(char *&buffer, unsigned long value) throw()
{
  buffer[0] = (value & 0xFF000000) >> 24;
  buffer[1] = (value & 0x00FF0000) >> 16;
  buffer[2] = (value & 0x0000FF00) >> 8;
  buffer[3] = (value & 0x000000FF) >> 0;
  buffer += 4;
}

dns_msg_header *
decode_hdr(const char *buffer)
{
  dns_msg_header *hdr = new dns_msg_header;
  hdr->id             = get16bits(buffer);
  uint fields         = get16bits(buffer);
  // hdr->qr      = fields & 0x8000;
  hdr->qr     = (fields >> 7) & 0x1;
  hdr->opcode = fields & 0x7800;
  hdr->aa     = fields & 0x0400;
  hdr->tc     = fields & 0x0200;
  hdr->rd     = fields & 0x0100;

  hdr->ra = fields & 0x8000;
  // z, ad, cd
  hdr->rcode   = (buffer[3] & 0x0F);
  hdr->qdCount = get16bits(buffer);
  hdr->anCount = get16bits(buffer);
  hdr->nsCount = get16bits(buffer);
  hdr->arCount = get16bits(buffer);
  return hdr;
}

dns_msg_question *
decode_question(const char *buffer)
{
  dns_msg_question *question = new dns_msg_question;
  std::string m_qName        = "";
  int length                 = *buffer++;
  // llarp::LogInfo("qNamLen", length);
  while(length != 0)
  {
    for(int i = 0; i < length; i++)
    {
      char c = *buffer++;
      m_qName.append(1, c);
    }
    length = *buffer++;
    if(length != 0)
      m_qName.append(1, '.');
  }
  question->name   = m_qName;
  question->type   = get16bits(buffer);
  question->qClass = get16bits(buffer);
  return question;
}

dns_msg_answer *
decode_answer(const char *buffer)
{
  dns_msg_answer *answer = new dns_msg_answer;
  answer->type           = get16bits(buffer);
  answer->aClass         = get16bits(buffer);
  answer->ttl            = get32bits(buffer);
  answer->rdLen          = get16bits(buffer);
  answer->rData          = get16bits(buffer);
  return answer;
}

void
code_domain(char *&buffer, const std::string &domain) throw()
{
  int start(0), end;  // indexes
  // llarp::LogInfo("domain [", domain, "]");
  while((end = domain.find('.', start)) != std::string::npos)
  {
    *buffer++ = end - start;  // label length octet
    for(int i = start; i < end; i++)
    {
      *buffer++ = domain[i];  // label octets
      // llarp::LogInfo("Writing ", domain[i], " at ", i);
    }
    start = end + 1;  // Skip '.'
  }

  // llarp::LogInfo("start ", start, " domain size ", domain.size());

  *buffer++ = domain.size() - start;  // last label length octet
  for(int i = start; i < domain.size(); i++)
  {
    *buffer++ = domain[i];  // last label octets
    // llarp::LogInfo("Writing ", domain[i], " at ", i);
  }

  *buffer++ = 0;
}

ssize_t
raw_sendto_dns_hook_func(void *sock, const struct sockaddr *from,
                         const void *buffer, size_t length)
{
  int *fd = (int *)sock;
  // how do we get to these??
  socklen_t addrLen = sizeof(struct sockaddr_in);
  return sendto(*fd, buffer, length, 0, from, addrLen);
}

ssize_t
llarp_sendto_dns_hook_func(void *sock, const struct sockaddr *from,
                           const void *buffer, size_t length)
{
  struct llarp_udp_io *udp = (struct llarp_udp_io *)sock;
  if(!udp)
  {
    llarp::LogWarn("couldnt cast to udp");
    return 0;
  }
  // llarp::LogInfo("hook sending ", udp, " bytes: ", length);
  // udp seems ok
  // this call isn't calling the function...
  // llarp::ev_io * evio = static_cast< llarp::ev_io * >(udp->impl);
  // printf("ev_io[%x]\n", evio);
  return llarp_ev_udp_sendto(udp, from, buffer, length);
}

bool
forward_dns_request(std::string request)
{
  return true;
}

void
writesend_dnss_response(struct sockaddr *hostRes, const struct sockaddr *from,
                        dns_request *request)
{
  lock_t lock(m_dnsd2_Mutex);
  if(!hostRes)
  {
    llarp::LogWarn("Failed to resolve");
    // FIXME: actually return correct packet
    return;
  }

  const size_t BUFFER_SIZE = 1024;
  char buf[BUFFER_SIZE];
  memset(buf, 0, BUFFER_SIZE);
  char *write_buffer = buf;
  char *bufferBegin  = buf;
  // build header
  put16bits(write_buffer, request->id);
  int fields = (1 << 15);  // QR => message type, 1 = response
  fields += (0 << 14);     // I think opcode is always 0
  fields += 0;             // response code (3 => not found, 0 = Ok)
  put16bits(write_buffer, fields);

  put16bits(write_buffer, 1);  // QD (number of questions)
  put16bits(write_buffer, 1);  // AN (number of answers)
  put16bits(write_buffer, 0);  // NS (number of auth RRs)
  put16bits(write_buffer, 0);  // AR (number of Additional RRs)

  // code question
  code_domain(write_buffer, request->m_qName);
  put16bits(write_buffer, request->m_qType);
  put16bits(write_buffer, request->m_qClass);

  // code answer
  code_domain(write_buffer, request->m_qName);  // com, type=6, ttl=0
  put16bits(write_buffer, request->m_qType);
  put16bits(write_buffer, request->m_qClass);
  put32bits(write_buffer, 1453);  // ttl

  // has to be a string of 4 bytes
  struct sockaddr_in *sin = (struct sockaddr_in *)hostRes;
  unsigned char *ip       = (unsigned char *)&sin->sin_addr.s_addr;

  put16bits(write_buffer, 4);  // rdLength
  *write_buffer++ = ip[0];
  *write_buffer++ = ip[1];
  *write_buffer++ = ip[2];
  *write_buffer++ = ip[3];

  uint out_bytes = write_buffer - bufferBegin;
  llarp::LogInfo("Sending ", out_bytes, " bytes");
  // struct llarp_udp_io *udp = (struct llarp_udp_io *)request->user;
  request->hook(request->user, from, buf, out_bytes);
}

void
handle_dnsc_result(dns_client_request *client_request)
{
  // llarp::LogInfo("phase2 client ", client_request);
  // writesend_dnss_response(struct sockaddr *hostRes, const struct sockaddr
  // *from, dns_request *request)
  dns_request *server_request = (dns_request *)client_request->user;
  // llarp::Addr test(*server_request->from);
  // llarp::LogInfo("server request sock ", server_request->from, " is ", test);
  // llarp::LogInfo("phase2 server ", server_request);
  writesend_dnss_response(
      client_request->found ? &client_request->result : nullptr,
      server_request->from, server_request);
  llarp_host_resolved(client_request);
}

// our generic version
void
handle_recvfrom(const char *buffer, ssize_t nbytes, const struct sockaddr *from,
                dns_request *request)
{
  lock_t lock(m_dnsd_Mutex);
  const size_t HDR_OFFSET = 12;
  const char *p_buffer    = buffer;

  int rcode = (buffer[3] & 0x0F);
  llarp::LogInfo("dnsd rcode ", rcode);

  dns_msg_header *msg = decode_hdr(p_buffer);
  // llarp::LogInfo("DNS_MSG size", sizeof(dns_msg));
  p_buffer += HDR_OFFSET;
  request->id         = msg->id;
  std::string m_qName = "";
  int length          = *p_buffer++;
  // llarp::LogInfo("qNamLen", length);
  while(length != 0)
  {
    for(int i = 0; i < length; i++)
    {
      char c = *p_buffer++;
      m_qName.append(1, c);
    }
    length = *p_buffer++;
    if(length != 0)
      m_qName.append(1, '.');
  }
  request->m_qName  = m_qName;
  request->m_qType  = get16bits(p_buffer);
  request->m_qClass = get16bits(p_buffer);
  llarp::LogInfo("qName  ", m_qName);
  llarp::LogInfo("qType  ", request->m_qType);
  llarp::LogInfo("qClass ", request->m_qClass);

  /*
  llarp::Addr test(*request->from);
  llarp::LogInfo("DNS request from ", test);
  llarp::Addr test2(from);
  llarp::LogInfo("DNS request from ", test2);
   */

  if(!forward_dns_request(m_qName))
  {
    // told that hook will handle overrides
    return;
  }

  sockaddr *hostRes = nullptr;
  // FIXME: how can we tell the mode?
  // struct llarp_udp_io *udp = static_cast<struct llarp_udp_io
  // *>(request->user);
  if(0)
  {
    hostRes = resolveHost(m_qName.c_str());
    llarp::Addr anIp(*hostRes);
    llarp::LogInfo("DNS got ", anIp);
    // writesend_dnss_response(struct sockaddr *hostRes, const struct sockaddr
    // *from, dns_request *request)
    sockaddr *fromCopy = new sockaddr(*from);
    writesend_dnss_response(hostRes, fromCopy, request);
  }
  else
  {
    // llarp::Addr anIp;
    // llarp::LogInfo("Checking server request ", request);
    struct llarp_udp_io *udp    = (struct llarp_udp_io *)request->user;
    struct dns_tracker *tracker = (struct dns_tracker *)udp->user;
    dnsd_context *dnsd          = tracker->dnsd;
    // dnsd_context *dnsd = (dnsd_context *)udp->user;
    // llarp::LogInfo("Server request UDP  ", request->user);
    // llarp::LogInfo("server request hook ", request->hook);
    // llarp::LogInfo("UDP ", udp);
    // hostRes = llarp_resolveHost(udp->parent, m_qName.c_str());
    llarp_resolve_host(&dnsd->client, m_qName.c_str(), &handle_dnsc_result,
                       (void *)request);
  }
}

// this is called in net threadpool
void
llarp_handle_recvfrom(struct llarp_udp_io *udp, const struct sockaddr *paddr,
                      const void *buf, ssize_t sz)
{
  lock_t lock(m_dnsd3_Mutex);
  // llarp_link *link = static_cast< llarp_link * >(udp->user);
  llarp::LogInfo("llarp Received Bytes ", sz);
  dns_request *llarp_dns_request = new dns_request;
  // llarp::LogInfo("Creating server request ", &llarp_dns_request);
  // llarp::LogInfo("Server UDP address ", udp);

  // make a copy of the sockaddr
  llarp_dns_request->from = new sockaddr(*paddr);
  llarp_dns_request->user = (void *)udp;
  // set sock hook
  llarp_dns_request->hook = &llarp_sendto_dns_hook_func;

  // llarp::LogInfo("Server request's UDP ", llarp_dns_request->user);
  handle_recvfrom((char *)buf, sz, llarp_dns_request->from, llarp_dns_request);
}

void
raw_handle_recvfrom(int *sockfd, const struct sockaddr *saddr, const void *buf,
                    ssize_t sz)
{
  llarp::LogInfo("raw Received Bytes ", sz);
  dns_request *llarp_dns_request = new dns_request;
  llarp_dns_request->from        = (struct sockaddr *)saddr;
  llarp_dns_request->user        = (void *)sockfd;
  llarp_dns_request->hook        = &raw_sendto_dns_hook_func;
  handle_recvfrom((char *)buf, sz, saddr, llarp_dns_request);
}

dns_tracker dns_udp_tracker;

bool
llarp_dnsd_init(struct dnsd_context *dnsd, struct llarp_ev_loop *netloop,
                const char *dnsd_ifname, uint16_t dnsd_port,
                const char *dnsc_hostname, uint16_t dnsc_port)
{
  struct sockaddr_in bindaddr;
  bindaddr.sin_addr.s_addr = inet_addr("0.0.0.0");
  bindaddr.sin_family      = AF_INET;
  bindaddr.sin_port        = htons(dnsd_port);

  dnsd->udp.user     = &dns_udp_tracker;
  dnsd->udp.recvfrom = &llarp_handle_dns_recvfrom;
  dnsd->udp.tick     = nullptr;

  dns_udp_tracker.dnsd = dnsd;

  // configure dns client
  if(!llarp_dnsc_init(&dnsd->client, &dnsd->udp, dnsc_hostname, dnsc_port))
  {
    llarp::LogError("Couldnt init dns client");
    return false;
  }

  return llarp_ev_add_udp(netloop, &dnsd->udp, (const sockaddr *)&bindaddr)
      != -1;
}
