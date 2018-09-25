#include <llarp/dnsc.hpp>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h> /* getaddrinfo, getnameinfo */
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <stdlib.h> /* exit */
#include <string.h> /* memset */
#include <sys/types.h>
#ifndef _MSC_VER
#include <unistd.h> /* close */
#endif

#include <algorithm>      // for std::find_if
#include <llarp/net.hpp>  // for llarp::Addr
#include <llarp/logger.hpp>
#include <stdio.h>  // sprintf

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

dns_tracker dns_udp_tracker;

#define DNC_BUF_SIZE 512
/// a question to be asked remotely (the actual bytes to send on the wire)
// header, question
struct dns_query
{
  uint16_t length;
  // char *url;
  unsigned char request[DNC_BUF_SIZE];
  // uint16_t reqType;
};

/// build a DNS question packet
struct dns_query *
build_dns_packet(char *url, uint16_t id, uint16_t reqType)
{
  dns_query *dnsQuery = new dns_query;
  dnsQuery->length    = 12;
  // ID
  // buffer[0] = (value & 0xFF00) >> 8;
  // buffer[1] = value & 0xFF;
  llarp::LogDebug("building request ", id);

  dnsQuery->request[0] = (id & 0xFF00) >> 8;
  dnsQuery->request[1] = (id & 0x00FF) >> 0;
  // field
  dnsQuery->request[2] = 0x01;
  dnsQuery->request[3] = 0x00;
  // questions
  dnsQuery->request[4] = 0x00;
  dnsQuery->request[5] = 0x01;
  // answers
  dnsQuery->request[6] = 0x00;
  dnsQuery->request[7] = 0x00;
  // ns
  dnsQuery->request[8] = 0x00;
  dnsQuery->request[9] = 0x00;
  // ar
  dnsQuery->request[10] = 0x00;
  dnsQuery->request[11] = 0x00;

  char *word;
  // llarp::LogDebug("Asking DNS server %s about %s", SERVER, dnsQuery->url);

  char *strTemp = strdup(url);

  word = strtok(strTemp, ".");
  while(word)
  {
    // llarp::LogDebug("parsing hostname: \"%s\" is %zu characters", word,
    // strlen(word));
    dnsQuery->request[dnsQuery->length++] = strlen(word);
    for(unsigned int i = 0; i < strlen(word); i++)
    {
      dnsQuery->request[dnsQuery->length++] = word[i];
    }
    word = strtok(nullptr, ".");
  }
  dnsQuery->request[dnsQuery->length++] = 0x00;  // End of the host name
  dnsQuery->request[dnsQuery->length++] =
      0x00;  // 0x0001 - Query is a Type A query (host address)
  dnsQuery->request[dnsQuery->length++] = reqType;
  dnsQuery->request[dnsQuery->length++] =
      0x00;  // 0x0001 - Query is class IN (Internet address)
  dnsQuery->request[dnsQuery->length++] = 0x01;
  return dnsQuery;
}

dns_query *
answer_request_alloc(struct dnsc_context *dnsc, void *sock, const char *url,
                     dnsc_answer_hook_func resolved, void *user)
{
  dnsc_answer_request *request = new dnsc_answer_request;
  if(!request)
  {
    llarp::LogError("Couldn't make dnsc request");
    return nullptr;
  }
  request->sock     = sock;
  request->user     = user;
  request->resolved = resolved;
  request->found    = false;
  request->context  = dnsc;

  char *sUrl             = strdup(url);
  request->question.name = (char *)sUrl;

  // leave 256 bytes available
  if(request->question.name.size() > 255)
  {
    // size_t diff = request->question.name.size() - 255;
    // request->question.name = request->question.name.substr(diff); // get the
    // rightmost 255 bytes
    llarp::LogWarn("dnsc request question too long");
    return nullptr;
  }
  request->question.type   = strstr(url, "in-addr.arpa") != nullptr ? 12 : 1;
  request->question.qClass = 1;

  // register our self with the tracker
  dns_tracker *tracker = request->context->tracker;
  uint16_t id          = ++tracker->c_requests;
  if(id == 65535)
    id = 0;
  tracker->client_request[id] = std::unique_ptr< dnsc_answer_request >(request);

  dns_query *dns_packet = build_dns_packet(
      (char *)request->question.name.c_str(), id, request->question.type);

  return dns_packet;
}

// FIXME: make first a std_unique
/// generic dnsc handler
void
generic_handle_dnsc_recvfrom(dnsc_answer_request *request,
                             const struct sockaddr *saddr, const void *buf,
                             ssize_t sz)
{
  // llarp::LogInfo("got a response, udp user is ", udp->user);

  unsigned char *castBuf = (unsigned char *)buf;
  // auto buffer            = llarp::StackBuffer< decltype(castBuf) >(castBuf);
  dns_msg_header *hdr = decode_hdr((const char *)castBuf);

  llarp::LogDebug("Header got client responses for id: ", hdr->id);
  if(!request)
  {
    llarp::LogError(
        "User data to DNS Client response not a dnsc_answer_request");
    // we can't call back the hook
    return;
  }
  // llarp_dnsc_unbind(request);

  if(sz < 0)
  {
    llarp::LogWarn("Error Receiving DNS Client Response");
    request->resolved(request);
    return;
  }

  // unsigned char *castBuf = (unsigned char *)buf;
  // auto buffer = llarp::StackBuffer< decltype(castBuf) >(castBuf);

  // hexdump("received packet", &buffer, ret);
  /*
   uint16_t QDCOUNT;   // No. of items in Question Section
   uint16_t ANCOUNT;   // No. of items in Answer Section
   uint16_t NSCOUNT;   // No. of items in Authority Section
   uint16_t ARCOUNT;   // No. of items in Additional Section
   uint16_t QCLASS;    // Specifies the class of the query
   uint16_t ATYPE;     // Specifies the meaning of the data in the RDATA field
   uint16_t ACLASS;    // Specifies the class of the data in the RDATA field
   uint32_t TTL;       // The number of seconds the results can be cached
   uint16_t RDLENGTH;  // The length of the RDATA field
   uint16_t MSGID;
   */
  uint8_t rcode;
  // int length;

  // struct dns_query *dnsQuery = &request->query;

  // rcode = (buffer[3] & 0x0F);
  // llarp::LogInfo("dnsc rcode ", rcode);

  dns_msg_header *msg = decode_hdr((const char *)castBuf);
  castBuf += 12;
  llarp::LogDebug("msg id ", msg->id);
  uint8_t qr = msg->qr;
  llarp::LogDebug("msg qr ", qr);
  uint8_t opcode = msg->opcode;
  llarp::LogDebug("msg op ", opcode);
  rcode = msg->rcode;
  llarp::LogDebug("msg rc ", rcode);

  llarp::LogDebug("msg qdc ", msg->qdCount);
  llarp::LogDebug("msg anc ", msg->anCount);
  llarp::LogDebug("msg nsc ", msg->nsCount);
  llarp::LogDebug("msg arc ", msg->arCount);

  // we may need to parse question first

  /*
   dns_msg_question *question = decode_question((const char *)castBuf);
   llarp::LogInfo("que name  ", question->name);
   castBuf += question->name.length() + 8;

   dns_msg_answer *answer = decode_answer((const char *)castBuf);
   castBuf += answer->name.length() + 4 + 4 + 4 + answer->rdLen;
   */

  // FIXME: only handling one atm
  dns_msg_question *question = nullptr;
  for(uint i = 0; i < hdr->qdCount; i++)
  {
    question = decode_question((const char *)castBuf);
    llarp::LogDebug("Read a question");
    // 1 dot: 1 byte for length + length
    // 4 bytes for class/type
    castBuf += question->name.length() + 1 + 4;
    castBuf += 2;  // skip answer label
  }

  // FIXME: only handling one atm
  std::vector< dns_msg_answer * > answers;
  dns_msg_answer *answer = nullptr;
  for(uint i = 0; i < hdr->anCount; i++)
  {
    answer = decode_answer((const char *)castBuf);
    answers.push_back(answer);
    llarp::LogDebug("Read an answer ", answer->type, " for ",
                    request->question.name);
    // llarp::LogInfo("Read an answer. Label Len: ", answer->name.length(), "
    // rdLen: ", answer->rdLen);
    // name + Type (2) + Class (2) + TTL (4) + rdLen (2) + rdData + skip next
    // answer label (1) first 2 was answer->name.length() if lbl is ref and type
    // 1: it should be 16 bytes long l0 + t2 + c2 + t4 + l2 + rd4 (14)   + l2
    // (2)
    castBuf += 0 + 2 + 2 + 4 + 2 + answer->rdLen;
    castBuf += 2;  // skip answer label
    uint8_t first = *castBuf;
    if(first != 0)
    {
      llarp::LogDebug("next byte isnt 12, skipping ahead one byte. ",
                      std::to_string(first));
      castBuf++;
    }
    // prevent reading past the end of the packet
    auto diff = castBuf - (unsigned char *)buf;
    llarp::LogDebug("Read answer, bytes left ", diff);
    if(diff > sz)
    {
      llarp::LogWarn("Would read past end of dns packet. for ",
                     request->question.name);
      break;
    }
  }

  // handle authority records (usually no answers with these, so we'll just
  // stomp) usually NS records tho
  for(uint i = 0; i < hdr->nsCount; i++)
  {
    answer = decode_answer((const char *)castBuf);
    // answers.push_back(answer);
    llarp::LogDebug("Read an authority");
    castBuf += answer->name.length() + 4 + 4 + 4 + answer->rdLen;
  }

  /*
  size_t i = 0;
  for(auto it = answers.begin(); it != answers.end(); ++it)
  {
    llarp::LogInfo("Answer #", i, " class: [", (*it)->aClass, "] type: [",
  (*it)->type,
                   "] rdlen[", (*it)->rdLen, "]");
    i++;
  }
  */

  // dns_msg_answer *answer2 = decode_answer((const char*)castBuf);
  // castBuf += answer->name.length() + 4 + 4 + 4 + answer->rdLen;

  // llarp::LogDebug("query type: %u\n", dnsQuery->reqType);
  /*
   QCLASS = (uint16_t)dnsQuery->request[dnsQuery->length - 2] * 0x100
   + dnsQuery->request[dnsQuery->length - 1];
   llarp::LogInfo("query class: ", QCLASS);

   length = dnsQuery->length + 1;  // to skip 0xc00c
   // printf("length [%d] from [%d]\n", length, buffer.base);
   ATYPE = (uint16_t)buffer[length + 1] * 0x100 + buffer[length + 2];
   llarp::LogInfo("answer type: ", ATYPE);
   ACLASS = (uint16_t)buffer[length + 3] * 0x100 + buffer[length + 4];
   llarp::LogInfo("answer class: ", ACLASS);
   TTL = (uint32_t)buffer[length + 5] * 0x1000000 + buffer[length + 6] * 0x10000
   + buffer[length + 7] * 0x100 + buffer[length + 8];
   llarp::LogInfo("seconds to cache: ", TTL);
   RDLENGTH = (uint16_t)buffer[length + 9] * 0x100 + buffer[length + 10];
   llarp::LogInfo("bytes in answer: ", RDLENGTH);

   MSGID = (uint16_t)buffer[0] * 0x100 + buffer[1];
   // llarp::LogDebug("answer msg id: %u\n", MSGID);
   */
  llarp::Addr upstreamAddr(*request->context->server);

  if(answer == nullptr)
  {
    llarp::LogWarn("nameserver ", upstreamAddr, " didnt return any answers:");
    request->resolved(request);
    return;
  }
  if(answer->type == 5)
  {
    llarp::LogInfo("Last answer is a cname, advancing to first");
    answer = answers.front();
  }

  llarp::LogDebug("ans class ", answer->aClass);
  llarp::LogDebug("ans type  ", answer->type);
  llarp::LogDebug("ans ttl   ", answer->ttl);
  llarp::LogDebug("ans rdlen ", answer->rdLen);

  /*
   llarp::LogInfo("ans2 class ", answer2->aClass);
   llarp::LogInfo("ans2 type  ", answer2->type);
   llarp::LogInfo("ans2 ttl   ", answer2->ttl);
   llarp::LogInfo("ans2 rdlen ", answer2->rdLen);
   */

  if(rcode == 2)
  {
    llarp::LogWarn("nameserver ", upstreamAddr, " returned SERVFAIL:");
    llarp::LogWarn(
        "  the name server was unable to process this query due to a problem "
        "with the name server.");
    request->resolved(request);
    return;
  }
  else if(rcode == 3)
  {
    llarp::LogWarn("nameserver ", upstreamAddr,
                   " returned NXDOMAIN for: ", request->question.name);
    llarp::LogWarn("  the domain name referenced in the query does not exist");
    request->resolved(request);
    return;
  }

  int ip = 0;

  /* search for and print IPv4 addresses */
  // if(dnsQuery->reqType == 0x01)
  if(request->question.type == 1)
  {
    // llarp::LogInfo("DNS server's answer is: (type#=", ATYPE, "):");
    llarp::LogDebug("IPv4 address(es) for ", request->question.name, ":");

    if(answer->rdLen == 4)
    {
      request->result.sa_family = AF_INET;
#if((__APPLE__ && __MACH__) || __FreeBSD__)
      request->result.sa_len = sizeof(in_addr);
#endif
      struct in_addr *addr =
          &((struct sockaddr_in *)&request->result)->sin_addr;

      unsigned char *ip = (unsigned char *)&(addr->s_addr);
      ip[0]             = answer->rData[0];
      ip[1]             = answer->rData[1];
      ip[2]             = answer->rData[2];
      ip[3]             = answer->rData[3];

      llarp::Addr test(request->result);
      llarp::LogDebug(test);
      request->found = true;
      request->resolved(request);
      return;
    }

    if(!ip)
    {
      llarp::LogWarn("  No IPv4 address found in the DNS answer!");
      request->resolved(request);
      return;
    }
  }
  else if(request->question.type == 12)
  {
    llarp::LogInfo("Resolving PTR");
    request->found  = true;
    request->revDNS = std::string((char *)answer->rData);
    request->resolved(request);
    return;
  }
}

void
raw_resolve_host(struct dnsc_context *dnsc, const char *url,
                 dnsc_answer_hook_func resolved, void *user)
{
  dns_query *dns_packet =
      answer_request_alloc(dnsc, nullptr, url, resolved, user);
  if(!dns_packet)
  {
    llarp::LogError("Couldn't make dnsc packet");
    return;
  }

  // char *word;
  llarp::Addr upstreamAddr(*dnsc->server);
  llarp::LogDebug("Asking DNS server ", upstreamAddr, " about ", url);

  struct sockaddr_in addr;
  ssize_t ret;
  socklen_t size;
  // int length;
  unsigned char buffer[DNC_BUF_SIZE];
#ifndef _WIN32
  int sockfd;
#else
  SOCKET sockfd;
#endif

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd < 0)
  {
    llarp::LogWarn("Error creating socket!\n");
    return;
  }
  // socket = sockfd;
  sockaddr_in *dnscSock = ((sockaddr_in *)dnsc->server);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = dnscSock->sin_addr.s_addr;
  addr.sin_port        = dnscSock->sin_port;
  size                 = sizeof(addr);

  // hexdump("sending packet", &dnsQuery.request, dnsQuery.length);

#ifdef _WIN32
  ret = sendto(sockfd, (const char *)dns_packet->request, dns_packet->length, 0,
               (struct sockaddr *)&addr, size);
#else
  ret = sendto(sockfd, (const char *)dns_packet->request, dns_packet->length, 0,
               (struct sockaddr *)&addr, size);
#endif

  delete dns_packet;
  if(ret < 0)
  {
    llarp::LogWarn("Error Sending Request");
    return;
  }
  llarp::LogInfo("Sent");

  memset(&buffer, 0, DNC_BUF_SIZE);
  llarp::LogInfo("Waiting for recv");

  // Timeout?
  ret = recvfrom(sockfd, (char*)buffer, DNC_BUF_SIZE, 0, (struct sockaddr *)&addr,
                 &size);
  llarp::LogInfo("recv done ", size);
  if(ret < 0)
  {
    llarp::LogWarn("Error Receiving Response");
    return;
  }
  llarp::LogInfo("closing new socket\n");

  // hexdump("received packet", &buffer, ret);

#ifndef _WIN32
  close(sockfd);
#else
  closesocket(sockfd);
#endif

  unsigned char *castBuf = (unsigned char *)buffer;
  // auto buffer            = llarp::StackBuffer< decltype(castBuf) >(castBuf);
  dns_msg_header *hdr = decode_hdr((const char *)castBuf);
  llarp::LogInfo("response header says it belongs to id #", hdr->id);

  // if we sent this out, then there's an id
  struct dns_tracker *tracker = (struct dns_tracker *)dnsc->tracker;
  generic_handle_dnsc_recvfrom(tracker->client_request[hdr->id].get(), nullptr,
                               castBuf, size);
}

/// intermediate udp_io handler
void
llarp_handle_dnsc_recvfrom(struct llarp_udp_io *udp,
                           const struct sockaddr *saddr, const void *buf,
                           ssize_t sz)
{
  unsigned char *castBuf = (unsigned char *)buf;
  // auto buffer            = llarp::StackBuffer< decltype(castBuf) >(castBuf);
  dns_msg_header *hdr = decode_hdr((const char *)castBuf);

  llarp::LogDebug("Header got client responses for id: ", hdr->id);

  // if we sent this out, then there's an id
  struct dns_tracker *tracker         = (struct dns_tracker *)udp->user;
  struct dnsc_answer_request *request = tracker->client_request[hdr->id].get();

  // sometimes we'll get double responses
  if(request)
  {
    generic_handle_dnsc_recvfrom(request, saddr, buf, sz);
  }
  else
  {
    llarp::LogWarn("Ignoring multiple responses on ID #", hdr->id);
  }
}

bool
llarp_resolve_host(struct dnsc_context *dnsc, const char *url,
                   dnsc_answer_hook_func resolved, void *user)
{
  dns_query *dns_packet =
      answer_request_alloc(dnsc, &dnsc->udp, url, resolved, user);
  if(!dns_packet)
  {
    llarp::LogError("Couldn't make dnsc packet");
    return false;
  }

  // register request with udp response tracker
  // dns_tracker *tracker = (dns_tracker *)dnsc->udp->user;

  /*
  uint16_t length = 0;
  dns_msg_header header;
  header.id         = htons(id);
  header.qr         = 0;
  header.opcode     = 0;
  header.aa         = 0;
  header.tc         = 0;
  header.rd         = 1;
  header.ra         = 0;
  header.rcode      = 0;
  header.qdCount    = htons(1);
  header.anCount    = 0;
  header.nsCount    = 0;
  header.arCount    = 0;
  length += 12;

  //request->question.name   = sUrl;
  request->question.type   = htons(1);
  request->question.qClass = htons(1);

  uint16_t qLen = request->question.name.length() + 8;
  length += qLen;

  unsigned char bytes[length];
  // memcpy isn't going to fix the network endian issue
  // encode header into bytes
  memcpy(bytes, &header, 12);
  // encode question into bytes
  memcpy(bytes + 12, &request->question, qLen);
  */

  // uint16_t id                 = ++tracker->c_requests;
  // tracker->client_request[id] = request;
  // llarp::LogInfo("Sending request #", tracker->c_requests, " ", length, "
  // bytes");

  // ssize_t ret = llarp_ev_udp_sendto(dnsc->udp, dnsc->server, bytes, length);
  ssize_t ret = llarp_ev_udp_sendto(dnsc->udp, dnsc->server,
                                    dns_packet->request, dns_packet->length);
  delete dns_packet;
  if(ret < 0)
  {
    llarp::LogWarn("Error Sending Request");
    return false;
  }

  return true;
}

void
llarp_host_resolved(dnsc_answer_request *request)
{
  dns_tracker *tracker = (dns_tracker *)request->context->tracker;
  auto val             = std::find_if(
      tracker->client_request.begin(), tracker->client_request.end(),
      [request](std::pair< const uint, std::unique_ptr< dnsc_answer_request > >
                    &element) { return element.second.get() == request; });
  if(val != tracker->client_request.end())
  {
    tracker->client_request[val->first].reset();
  }
  else
  {
    llarp::LogWarn("Couldn't disable ", request);
  }
  // delete request;
}

bool
llarp_dnsc_init(struct dnsc_context *dnsc, struct llarp_logic *logic,
                struct llarp_udp_io *udp, const char *dnsc_hostname,
                uint16_t dnsc_port)
{
  sockaddr_in *trgaddr     = new sockaddr_in;
  trgaddr->sin_addr.s_addr = inet_addr(dnsc_hostname);
  trgaddr->sin_port        = htons(dnsc_port);
  trgaddr->sin_family      = AF_INET;
  dnsc->server             = (sockaddr *)trgaddr;
  dnsc->udp                = udp;
  dnsc->tracker            = &dns_udp_tracker;
  dnsc->logic              = logic;
  return true;
}

bool
llarp_dnsc_stop(struct dnsc_context *dnsc)
{
  delete(sockaddr_in *)dnsc->server;  // deallocation
  return true;
}
