#include <llarp/dnsd.hpp>
#include <llarp/net.hpp>

extern dns_tracker dns_udp_tracker;

#ifdef _WIN32
#define wmin(x, y) (((x) < (y)) ? (x) : (y))
#define MIN wmin
#endif

ssize_t
raw_sendto_dns_hook_func(void *sock, const struct sockaddr *from,
                         const void *buffer, size_t length)
{
  int *fd = (int *)sock;
  // how do we get to these??
  socklen_t addrLen = sizeof(struct sockaddr_in);
  return sendto(*fd, (const char *)buffer, length, 0, from, addrLen);
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

void
write404_dnss_response(const struct sockaddr *from,
                       dnsd_question_request *request)
{
  const size_t BUFFER_SIZE = 1024 + (request->question.name.size() * 2);
  char buf[BUFFER_SIZE];
  memset(buf, 0, BUFFER_SIZE);
  char *write_buffer = buf;
  char *bufferBegin  = buf;
  // build header
  put16bits(write_buffer, request->id);
  int fields = (1 << 15);  // QR => message type, 1 = response
  fields += (0 << 14);     // I think opcode is always 0
  fields += 3;             // response code (3 => not found, 0 = Ok)
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

  put16bits(write_buffer, 1);  // rdLength
  *write_buffer++ = 0;         // write a null byte

  uint32_t out_bytes = write_buffer - bufferBegin;
  llarp::LogDebug("Sending 404, ", out_bytes, " bytes");
  // struct llarp_udp_io *udp = (struct llarp_udp_io *)request->user;
  request->sendto_hook(request->user, from, buf, out_bytes);
}

void
writecname_dnss_response(std::string cname, const struct sockaddr *from,
                         dnsd_question_request *request)
{
  const size_t BUFFER_SIZE = 1024 + (request->question.name.size() * 2);
  char buf[BUFFER_SIZE];  // heh, another UNIX compiler extension: VLAs in C++
  memset(buf, 0, BUFFER_SIZE);
  char *write_buffer = buf;
  char *bufferBegin  = buf;
  // build header
  put16bits(write_buffer, request->id);
  int fields = (1 << 15);  // QR => message type, 1 = response
  fields += (0 << 14);     // I think opcode is always 0
  fields += 0;             // response code (3 => not found, 0 = Ok)
  // fields |= 1UL << 7; // RA recursion available
  // fields |= 1UL << 8; // RD recursion desired
  // fields |= 1UL << 9; // 9 is truncate, forces TCP
  put16bits(write_buffer, fields);

  put16bits(write_buffer, 1);  // QD (number of questions)
  put16bits(write_buffer, 1);  // AN (number of answers)
  put16bits(write_buffer, 1);  // NS (number of auth RRs)
  put16bits(write_buffer, 1);  // AR (number of Additional RRs)

  // code question
  code_domain(write_buffer, request->question.name);
  put16bits(write_buffer, request->question.type);
  put16bits(write_buffer, request->question.qClass);

  // code answer
  code_domain(write_buffer, request->question.name);  // com, type=6, ttl=0
  put16bits(write_buffer, 5);                         // cname
  put16bits(write_buffer, request->question.qClass);
  put32bits(write_buffer, 1);  // ttl

  put16bits(write_buffer, cname.length() + 2);  // rdLength
  code_domain(write_buffer, cname);             // com, type=6, ttl=0
  // location of cname
  //*write_buffer++ = ip[0];
  //*write_buffer++ = ip[1];

  // write auth RR
  code_domain(write_buffer, cname);  // com, type=6, ttl=0
  put16bits(write_buffer, 2);        // NS
  put16bits(write_buffer, request->question.qClass);
  put32bits(write_buffer, 1);  // ttl

  std::string local("ns1.loki");
  put16bits(write_buffer, local.length() + 2);  // rdLength
  code_domain(write_buffer, local);             // com, type=6, ttl=0

  // write addl RR
  code_domain(write_buffer, local);  // com, type=6, ttl=0
  put16bits(write_buffer, 1);        // A
  put16bits(write_buffer, request->question.qClass);
  put32bits(write_buffer, 1);  // ttl

  put16bits(write_buffer, 4);  // rdLength
  *write_buffer++ = 127;
  *write_buffer++ = 0;
  *write_buffer++ = 0;
  *write_buffer++ = 1;

  uint32_t out_bytes = write_buffer - bufferBegin;
  llarp::LogDebug("Sending cname, ", out_bytes, " bytes");
  // struct llarp_udp_io *udp = (struct llarp_udp_io *)request->user;
  request->sendto_hook(request->user, from, buf, out_bytes);
}

void
writesend_dnss_revresponse(std::string reverse, const struct sockaddr *from,
                           dnsd_question_request *request)
{
  const size_t BUFFER_SIZE = 1500;
  char buf[BUFFER_SIZE] = {0};
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
  put32bits(write_buffer, 1);                     // ttl
  put16bits(write_buffer, reverse.length() + 2);  // rdLength
  code_domain(write_buffer, reverse);

  uint32_t out_bytes = write_buffer - bufferBegin;
  llarp::LogDebug("Sending reverse: ", reverse, " ", out_bytes, " bytes");
  // struct llarp_udp_io *udp = (struct llarp_udp_io *)request->user;
  request->sendto_hook(request->user, from, buf, out_bytes);
}

// FIXME: we need an DNS answer not a sockaddr
// otherwise ttl, type and class can't be relayed correctly
void
writesend_dnss_response(llarp::huint32_t *hostRes, const struct sockaddr *from,
                        dnsd_question_request *request)
{
  // llarp::Addr test(*from);
  // llarp::LogInfo("from ", test);
  if(!hostRes)
  {
    llarp::LogWarn("Failed to resolve ", request->question.name);
    write404_dnss_response(from, request);
    return;
  }

  const size_t BUFFER_SIZE = 1024 + (request->question.name.size() * 2);
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
  llarp::LogDebug("Sending question name: ", request->question.name);
  code_domain(write_buffer, request->question.name);  // com, type=6, ttl=0
  put16bits(write_buffer, request->question.type);
  put16bits(write_buffer, request->question.qClass);
  put32bits(write_buffer, 1);  // ttl

  put16bits(write_buffer, 4);  // rdLength
  // put32bits(write_buffer, xhtonl(*hostRes).n);
  put32bits(write_buffer, hostRes->h);

  // has to be a string of 4 bytes
  /*
  struct sockaddr_in *sin = (struct sockaddr_in *)hostRes;
  unsigned char *ip       = (unsigned char *)&sin->sin_addr.s_addr;

  put16bits(write_buffer, 4);  // rdLength
  llarp::LogDebug("Sending ip: ", std::to_string(ip[0]), '.',
                  std::to_string(ip[1]), '.', std::to_string(ip[2]), '.',
                  std::to_string(ip[3]));
  *write_buffer++ = ip[0];
  *write_buffer++ = ip[1];
  *write_buffer++ = ip[2];
  *write_buffer++ = ip[3];
  */

  uint32_t out_bytes = write_buffer - bufferBegin;
  llarp::LogDebug("Sending found, ", out_bytes, " bytes");
  // struct llarp_udp_io *udp = (struct llarp_udp_io *)request->user;
  request->sendto_hook(request->user, from, buf, out_bytes);
}

void
writesend_dnss_mxresponse(uint16_t priority, std::string mx,
                          const struct sockaddr *from,
                          dnsd_question_request *request)
{
  const size_t BUFFER_SIZE = 1024 + (request->question.name.size() * 2);
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
  llarp::LogDebug("Sending question name: ", request->question.name);
  code_domain(write_buffer, request->question.name);  // com, type=6, ttl=0
  put16bits(write_buffer, request->question.type);
  put16bits(write_buffer, request->question.qClass);
  put32bits(write_buffer, 1);  // ttl

  put16bits(write_buffer, 2 + (mx.size() + 2));  // rdLength
  put16bits(write_buffer, priority);             // priority
  code_domain(write_buffer, mx);                 //

  uint32_t out_bytes = write_buffer - bufferBegin;
  llarp::LogDebug("Sending found, ", out_bytes, " bytes");
  // struct llarp_udp_io *udp = (struct llarp_udp_io *)request->user;
  request->sendto_hook(request->user, from, buf, out_bytes);
}

void
writesend_dnss_txtresponse(std::string txt, const struct sockaddr *from,
                           dnsd_question_request *request)
{
  const size_t BUFFER_SIZE = 1024 + (request->question.name.size() * 2);
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
  llarp::LogDebug("Sending question name: ", request->question.name);
  code_domain(write_buffer, request->question.name);  // com, type=6, ttl=0
  put16bits(write_buffer, request->question.type);
  put16bits(write_buffer, request->question.qClass);
  put32bits(write_buffer, 1);  // ttl

  put16bits(write_buffer, txt.size() + 2);  // rdLength
  *write_buffer = txt.size();               // write size
  write_buffer++;
  code_domain(write_buffer, txt);  //

  uint32_t out_bytes = write_buffer - bufferBegin;
  llarp::LogDebug("Sending found, ", out_bytes, " bytes");
  // struct llarp_udp_io *udp = (struct llarp_udp_io *)request->user;
  request->sendto_hook(request->user, from, buf, out_bytes);
}

void
handle_dnsc_result(dnsc_answer_request *client_request)
{
  dnsd_question_request *server_request =
      (dnsd_question_request *)client_request->user;
  if(!server_request)
  {
    llarp::LogError("Couldn't map client requser user to a server request");
    return;
  }
  // llarp::LogDebug("handle_dnsc_result - client request question type",
  // std::to_string(client_request->question.type));
  if(client_request->question.type == 12)
  {
    writesend_dnss_revresponse(client_request->revDNS, server_request->from,
                               server_request);
  }
  else if(client_request->question.type == 15)
  {
    writesend_dnss_mxresponse(client_request->result.h, client_request->revDNS,
                              server_request->from, server_request);
  }
  else if(client_request->question.type == 16)
  {
    llarp::LogInfo("Writing TXT ", client_request->revDNS);
    writesend_dnss_txtresponse(client_request->revDNS, server_request->from,
                               server_request);
  }
  else
  {
    if(client_request->question.type != 1)
    {
      llarp::LogInfo("Returning type ", client_request->question.type,
                     " as standard");
    }
    llarp::huint32_t *useHostRes = nullptr;
    if(client_request->found)
      useHostRes = &client_request->result;
    writesend_dnss_response(useHostRes, server_request->from, server_request);
  }
  llarp_host_resolved(client_request);
}

// our generic version
void
handle_recvfrom(const char *buffer, ssize_t nbytes, const struct sockaddr *from,
                dnsd_question_request *request)
{
  const size_t HDR_OFFSET = 12;
  const char *p_buffer    = buffer;

  int rcode = (buffer[3] & 0x0F);
  llarp::LogDebug("dnsd rcode ", rcode);

  dns_msg_header *msg = decode_hdr(p_buffer);
  p_buffer += HDR_OFFSET;
  request->id         = msg->id;
  std::string m_qName = "";
  int length          = *p_buffer++;
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

  llarp::LogDebug("qName  ", request->question.name);
  llarp::LogDebug("qType  ", request->question.type);
  llarp::LogDebug("qClass ", request->question.qClass);

  /*
  llarp::Addr test(*request->from);
  llarp::LogInfo("DNS request from ", test);
  llarp::Addr test2(from);
  llarp::LogInfo("DNS request from ", test2);
   */

  sockaddr *fromCopy =
      new sockaddr(*from);  // make our own sockaddr that won't get cleaned up
  if(!request)
  {
    llarp::LogError("request is not configured");
    return;
  }
  if(!request->context)
  {
    llarp::LogError("request context is not configured");
    return;
  }
  if(request->context->intercept)
  {
    // llarp::Addr test(*from);
    // llarp::LogInfo("from ", test);
    dnsd_query_hook_response *intercept =
        request->context->intercept(request->question.name, fromCopy, request);
    // if(!forward_dns_request(m_qName))
    if(intercept != nullptr)
    {
      llarp::LogDebug("hook returned a response");
      if(intercept->dontSendResponse)
      {
        llarp::LogDebug("HOOKED: Not sending a response");
        return;
      }
      if(intercept->dontLookUp == true && intercept->returnThis)
      {
        llarp::LogDebug("HOOKED: sending an immediate override");
        // told that hook will handle overrides
        writesend_dnss_response(intercept->returnThis, fromCopy, request);
        return;
      }
    }
  }

  // FIXME: check request and context's client
  if(!request->context)
  {
    llarp::LogError("dnsd request context was not a dnsd context");
    writesend_dnss_response(nullptr, fromCopy, request);
    return;
  }
  /*
  struct dns_tracker *tracker = (struct dns_tracker *)request->context->tracker;
  if (!tracker)
  {
    llarp::LogError("dnsd request context tracker was not a dns_tracker");
    sockaddr *fromCopy = new sockaddr(*from);
    writesend_dnss_response(nullptr, fromCopy, request);
    return;
  }
  dnsd_context *dnsd          = tracker->dnsd;
  if (!dnsd)
  {
    llarp::LogError("tracker dnsd was not a dnsd context");
    sockaddr *fromCopy = new sockaddr(*from);
    writesend_dnss_response(nullptr, fromCopy, request);
    return;
  }
  */
  delete fromCopy;
  if(request->llarp)
  {
    // make async request
    llarp_resolve_host(&request->context->client, m_qName.c_str(),
                       &handle_dnsc_result, (void *)request,
                       request->question.type);
  }
  else
  {
    // make raw/sync request
    raw_resolve_host(&request->context->client, m_qName.c_str(),
                     &handle_dnsc_result, (void *)request,
                     request->question.type);
  }
}

void
llarp_handle_dnsd_recvfrom(struct llarp_udp_io *udp,
                           const struct sockaddr *saddr, const void *buf,
                           ssize_t sz)
{
  if(!dns_udp_tracker.dnsd)
  {
    llarp::LogError("No tracker set in dnsd context");
    return;
  }
  // create new request
  dnsd_question_request *llarp_dns_request = new dnsd_question_request;
  llarp_dns_request->context = dns_udp_tracker.dnsd;  // set context
  llarp_dns_request->from =
      new sockaddr(*saddr);  // make a copy of the sockaddr
  llarp_dns_request->user  = (void *)udp;
  llarp_dns_request->llarp = true;
  llarp_dns_request->sendto_hook =
      &llarp_sendto_dns_hook_func;  // set sock hook

  // llarp::LogInfo("Server request's UDP ", llarp_dns_request->user);
  handle_recvfrom((char *)buf, sz, llarp_dns_request->from, llarp_dns_request);
}

void
raw_handle_recvfrom(int *sockfd, const struct sockaddr *saddr, const void *buf,
                    ssize_t sz)
{
  if(!dns_udp_tracker.dnsd)
  {
    llarp::LogError("No tracker set in dnsd context");
    return;
  }
  dnsd_question_request *llarp_dns_request = new dnsd_question_request;
  llarp_dns_request->context = dns_udp_tracker.dnsd;  // set context
  llarp_dns_request->from =
      new sockaddr(*saddr);  // make a copy of the sockaddr
  llarp_dns_request->user        = (void *)sockfd;
  llarp_dns_request->llarp       = false;
  llarp_dns_request->sendto_hook = &raw_sendto_dns_hook_func;
  handle_recvfrom((char *)buf, sz, llarp_dns_request->from, llarp_dns_request);
}

bool
llarp_dnsd_init(struct dnsd_context *const dnsd,
                struct llarp_logic *const logic,
                struct llarp_ev_loop *const netloop,
                const llarp::Addr &dnsd_sockaddr,
                const llarp::Addr &dnsc_sockaddr)
{
  // struct sockaddr_in bindaddr;
  /*
  llarp::Addr dnsd_addr;
  bool ifRes = GetIFAddr(std::string(dnsd_ifname), dnsd_addr, AF_INET);
  if (!ifRes)
  {
    llarp::LogError("Couldn't init dns server, can't resolve interface: ",
  dnsd_ifname); return false;
  }
  llarp::LogInfo("DNSd interface ip ", dnsd_addr);
  */

  /*
  bindaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // network byte
  bindaddr.sin_family      = AF_INET;
  bindaddr.sin_port        = htons(dnsd_port);
  */

  dnsd->udp.user     = &dns_udp_tracker;
  dnsd->udp.recvfrom = &llarp_handle_dns_recvfrom;
  dnsd->udp.tick     = nullptr;

  dns_udp_tracker.dnsd = dnsd;

  dnsd->tracker   = &dns_udp_tracker;  // register global tracker with context
  dnsd->intercept = nullptr;           // set default intercepter

  // set up fresh socket
  dnsd->client.udp           = new llarp_udp_io;
  dnsd->client.udp->user     = &dns_udp_tracker;
  dnsd->client.udp->recvfrom = &llarp_handle_dns_recvfrom;
  dnsd->client.udp->tick     = nullptr;

  // configure dns client
  // llarp::LogInfo("DNSd setting relay to ", dnsc_sockaddr);
  if(!llarp_dnsc_init(&dnsd->client, logic, netloop, dnsc_sockaddr))
  {
    llarp::LogError("Couldnt init dns client");
    return false;
  }

  if(netloop)
  {
    llarp::LogInfo("DNSd binding to ", dnsd_sockaddr);
    return llarp_ev_add_udp(netloop, &dnsd->udp,
                            (const sockaddr *)dnsd_sockaddr)
        != -1;
  }
  else
  {
    return true;
  }
}
