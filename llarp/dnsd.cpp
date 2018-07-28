#include "dnsd.hpp"
#include <llarp/dns.h>
#include <string>
#include "ev.hpp"
#include "logger.hpp"
#include "net.hpp"

dns_tracker dns_udp_tracker;

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

// FIXME: we need an DNS answer not a sockaddr
// otherwise ttl, type and class can't be relayed correctly
void
writesend_dnss_response(struct sockaddr *hostRes, const struct sockaddr *from,
                        dnsd_question_request *request)
{
  // lock_t lock(m_dnsd2_Mutex);
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
  code_domain(write_buffer, request->question.name);
  put16bits(write_buffer, request->question.type);
  put16bits(write_buffer, request->question.qClass);

  // code answer
  code_domain(write_buffer, request->question.name);  // com, type=6, ttl=0
  put16bits(write_buffer, request->question.type);
  put16bits(write_buffer, request->question.qClass);
  put32bits(write_buffer, 1);  // ttl

  // has to be a string of 4 bytes
  struct sockaddr_in *sin = (struct sockaddr_in *)hostRes;
  unsigned char *ip       = (unsigned char *)&sin->sin_addr.s_addr;

  put16bits(write_buffer, 4);  // rdLength
  *write_buffer++ = ip[0];
  *write_buffer++ = ip[1];
  *write_buffer++ = ip[2];
  *write_buffer++ = ip[3];

  uint out_bytes = write_buffer - bufferBegin;
  llarp::LogDebug("Sending ", out_bytes, " bytes");
  // struct llarp_udp_io *udp = (struct llarp_udp_io *)request->user;
  request->hook(request->user, from, buf, out_bytes);
}

void
handle_dnsc_result(dnsc_answer_request *client_request)
{
  // llarp::LogInfo("phase2 client ", client_request);
  // writesend_dnss_response(struct sockaddr *hostRes, const struct sockaddr
  // *from, dnsd_question_request *request)
  dnsd_question_request *server_request =
      (dnsd_question_request *)client_request->user;
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
                dnsd_question_request *request)
{
  // lock_t lock(m_dnsd_Mutex);
  const size_t HDR_OFFSET = 12;
  const char *p_buffer    = buffer;

  int rcode = (buffer[3] & 0x0F);
  llarp::LogDebug("dnsd rcode ", rcode);

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
  request->question.name   = m_qName;
  request->question.type   = get16bits(p_buffer);
  request->question.qClass = get16bits(p_buffer);

  // request->m_qName  = m_qName;
  // request->m_qType  = request->question.type;
  // request->m_qClass = request->question.qClass;
  llarp::LogDebug("qName  ", request->question.name);
  llarp::LogDebug("qType  ", request->question.type);
  llarp::LogDebug("qClass ", request->question.qClass);

  /*
  llarp::Addr test(*request->from);
  llarp::LogInfo("DNS request from ", test);
  llarp::Addr test2(from);
  llarp::LogInfo("DNS request from ", test2);
   */

  if(request->context->intercept)
  {
    sockaddr *intercept = request->context->intercept(request->question.name, request->context);
    // if(!forward_dns_request(m_qName))
    if(intercept != nullptr)
    {
      // told that hook will handle overrides
      sockaddr *fromCopy = new sockaddr(*from);
      writesend_dnss_response(intercept, fromCopy, request);
      return;
    }
  }

  sockaddr *hostRes = nullptr;
  if(request->llarp)
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
  else
  {
    hostRes = raw_resolve_host(m_qName.c_str());
    llarp::Addr anIp(*hostRes);
    llarp::LogDebug("DNSc got ", anIp);
    // writesend_dnss_response(struct sockaddr *hostRes, const struct sockaddr
    // *from, dnsd_question_request *request)
    sockaddr *fromCopy = new sockaddr(*from);
    writesend_dnss_response(hostRes, fromCopy, request);
  }
}

void
llarp_handle_dnsd_recvfrom(struct llarp_udp_io *udp,
                           const struct sockaddr *paddr, const void *buf,
                           ssize_t sz)
{
  // lock_t lock(m_dnsd3_Mutex);
  // llarp_link *link = static_cast< llarp_link * >(udp->user);
  llarp::LogDebug("llarp Received Bytes ", sz);
  dnsd_question_request *llarp_dns_request = new dnsd_question_request;
  // llarp::LogInfo("Creating server request ", &llarp_dns_request);
  // llarp::LogInfo("Server UDP address ", udp);
  llarp_dns_request->context = dns_udp_tracker.dnsd;
  // make a copy of the sockaddr
  llarp_dns_request->from  = new sockaddr(*paddr);
  llarp_dns_request->user  = (void *)udp;
  llarp_dns_request->llarp = true;
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
  dnsd_question_request *llarp_dns_request = new dnsd_question_request;
  llarp_dns_request->from                  = (struct sockaddr *)saddr;
  llarp_dns_request->user                  = (void *)sockfd;
  llarp_dns_request->llarp                 = false;
  llarp_dns_request->hook                  = &raw_sendto_dns_hook_func;
  handle_recvfrom((char *)buf, sz, saddr, llarp_dns_request);
}

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

  dnsd->intercept = nullptr;

  // configure dns client
  if(!llarp_dnsc_init(&dnsd->client, &dnsd->udp, dnsc_hostname, dnsc_port))
  {
    llarp::LogError("Couldnt init dns client");
    return false;
  }

  return llarp_ev_add_udp(netloop, &dnsd->udp, (const sockaddr *)&bindaddr)
      != -1;
}
