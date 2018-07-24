#include "dnsd.hpp"

#include <netdb.h>  /* getaddrinfo, getnameinfo */
#include <stdlib.h> /* exit */
#include <string.h> /* memset */
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> /* close */

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstdio>

#include <llarp/dns.h>
#include "logger.hpp"

// FIXME: make configurable
#define SERVER "8.8.8.8"
#define PORT 53

struct sockaddr *
resolveHost(const char *url)
{
  char *sUrl = strdup(url);
  struct dns_query dnsQuery;
  dnsQuery.length  = 12;
  dnsQuery.url     = sUrl;
  dnsQuery.reqType = 0x01;
  // dnsQuery.request  = { 0xDB, 0x42, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  // 0x00, 0x00, 0x00 };
  dnsQuery.request[0]  = 0xDB;
  dnsQuery.request[1]  = 0x42;
  dnsQuery.request[2]  = 0x01;
  dnsQuery.request[3]  = 0x00;
  dnsQuery.request[4]  = 0x00;
  dnsQuery.request[5]  = 0x01;
  dnsQuery.request[6]  = 0x00;
  dnsQuery.request[7]  = 0x00;
  dnsQuery.request[8]  = 0x00;
  dnsQuery.request[9]  = 0x00;
  dnsQuery.request[10] = 0x00;
  dnsQuery.request[11] = 0x00;

  char *word;
  unsigned int i;
  llarp::LogDebug("Asking DNS server %s about %s\n", SERVER, url);
  // dnsQuery.reqType = 0x01;

  word = strtok(sUrl, ".");
  while(word)
  {
    llarp::LogDebug("parsing hostname: \"%s\" is %zu characters\n", word,
                    strlen(word));
    dnsQuery.request[dnsQuery.length++] = strlen(word);
    for(i = 0; i < strlen(word); i++)
    {
      dnsQuery.request[dnsQuery.length++] = word[i];
    }
    word = strtok(NULL, ".");
  }

  dnsQuery.request[dnsQuery.length++] = 0x00;  // End of the host name
  dnsQuery.request[dnsQuery.length++] =
      0x00;  // 0x0001 - Query is a Type A query (host address)
  dnsQuery.request[dnsQuery.length++] = dnsQuery.reqType;
  dnsQuery.request[dnsQuery.length++] =
      0x00;  // 0x0001 - Query is class IN (Internet address)
  dnsQuery.request[dnsQuery.length++] = 0x01;

  struct sockaddr_in addr;
  // int socket;
  ssize_t ret;
  int rcode;
  socklen_t size;
  int ip = 0;
  int length;
  unsigned char buffer[DNC_BUF_SIZE];
  // unsigned char tempBuf[3];
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

  int sockfd;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd < 0)
  {
    llarp::LogWarn("Error creating socket!\n");
    return nullptr;
  }
  // socket = sockfd;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = inet_addr(SERVER);
  addr.sin_port        = htons(PORT);
  size                 = sizeof(addr);

  // hexdump("sending packet", &dnsQuery.request, dnsQuery.length);
  ret = sendto(sockfd, dnsQuery.request, dnsQuery.length, 0,
               (struct sockaddr *)&addr, size);
  if(ret < 0)
  {
    llarp::LogWarn("Error Sending Request");
    return nullptr;
  }
  // printf("Sent\n");

  memset(&buffer, 0, DNC_BUF_SIZE);
  ret = recvfrom(sockfd, buffer, DNC_BUF_SIZE, 0, (struct sockaddr *)&addr,
                 &size);
  if(ret < 0)
  {
    llarp::LogWarn("Error Receiving Response");
    return nullptr;
  }

  // hexdump("received packet", &buffer, ret);

  close(sockfd);

  rcode = (buffer[3] & 0x0F);

  // tempBuf[0] = buffer[4];
  // tempBuf[1] = buffer[5];
  // tempBuf[2] = '\0';

  // printf("%0x %0x %0x %0x\n", buffer[4], buffer[5], tempBuf[0], tempBuf[1]);

  // QDCOUNT = (uint16_t) strtol(tempBuf, NULL, 16);
  QDCOUNT = (uint16_t)buffer[4] * 0x100 + buffer[5];
  llarp::LogDebug("entries in question section: %u\n", QDCOUNT);
  ANCOUNT = (uint16_t)buffer[6] * 0x100 + buffer[7];
  llarp::LogDebug("records in answer section: %u\n", ANCOUNT);
  NSCOUNT = (uint16_t)buffer[8] * 0x100 + buffer[9];
  llarp::LogDebug("name server resource record count: %u\n", NSCOUNT);
  ARCOUNT = (uint16_t)buffer[10] * 0x100 + buffer[11];
  llarp::LogDebug("additional records count: %u\n", ARCOUNT);

  llarp::LogDebug("query type: %u\n", dnsQuery.reqType);
  QCLASS = (uint16_t)dnsQuery.request[dnsQuery.length - 2] * 0x100
      + dnsQuery.request[dnsQuery.length - 1];
  llarp::LogDebug("query class: %u\n", QCLASS);
  length = dnsQuery.length + 1;  // to skip 0xc00c
  ATYPE  = (uint16_t)buffer[length + 1] * 0x100 + buffer[length + 2];
  llarp::LogDebug("answer type: %u\n", ATYPE);
  ACLASS = (uint16_t)buffer[length + 3] * 0x100 + buffer[length + 4];
  llarp::LogDebug("answer class: %u\n", ACLASS);
  TTL = (uint32_t)buffer[length + 5] * 0x1000000 + buffer[length + 6] * 0x10000
      + buffer[length + 7] * 0x100 + buffer[length + 8];
  llarp::LogDebug("seconds to cache: %u\n", TTL);
  RDLENGTH = (uint16_t)buffer[length + 9] * 0x100 + buffer[length + 10];
  llarp::LogDebug("bytes in answer: %u\n", RDLENGTH);
  MSGID = (uint16_t)buffer[0] * 0x100 + buffer[1];
  llarp::LogDebug("answer msg id: %u\n", MSGID);

  if(rcode == 2)
  {
    llarp::LogWarn("nameserver %s returned SERVFAIL:\n", SERVER);
    llarp::LogWarn(
        "  the name server was unable to process this query due to a\n  "
        "problem with the name server.\n");
    return nullptr;
  }
  else if(rcode == 3)
  {
    llarp::LogWarn("nameserver %s returned NXDOMAIN for %s:\n", SERVER,
                   dnsQuery.url);
    llarp::LogWarn(
        "  the domain name referenced in the query does not exist\n");
    return nullptr;
  }

  /* search for and print IPv4 addresses */
  if(dnsQuery.reqType == 0x01)
  {
    llarp::LogDebug("DNS server's answer is: (type#=%u):", ATYPE);
    // printf("IPv4 address(es) for %s:\n", dnsQuery.url);
    for(i = 0; i < ret; i++)
    {
      if(buffer[i] == 0xC0 && buffer[i + 3] == 0x01)
      {
        ip++;
        i += 12; /* ! += buf[i+1]; */
        llarp::LogDebug(" %u.%u.%u.%u\n", buffer[i], buffer[i + 1],
                        buffer[i + 2], buffer[i + 3]);
        struct sockaddr *g_addr = new sockaddr;
        g_addr->sa_family       = AF_INET;
        // g_addr->sa_len          = sizeof(in_addr);
        struct in_addr *addr = &((struct sockaddr_in *)g_addr)->sin_addr;
        unsigned char *ip;

        // have ip point to s_addr
        ip = (unsigned char *)&(addr->s_addr);

        ip[0] = buffer[i + 0];
        ip[1] = buffer[i + 1];
        ip[2] = buffer[i + 2];
        ip[3] = buffer[i + 3];

        return g_addr;
      }
    }

    if(!ip)
    {
      llarp::LogWarn("  No IPv4 address found in the DNS response!\n");
      return nullptr;
    }
  }
  return nullptr;
}

void
llarp_handle_dnsclient_recvfrom(struct llarp_udp_io *udp,
                                const struct sockaddr *saddr, const void *buf,
                                ssize_t sz)
{
  struct dns_client_request *request = (struct dns_client_request *)udp->user;
  if(!request)
  {
    llarp::LogError(
        "User data to DNS Client response not a dns_client_request");
    // we can't call back the hook
    return;
  }
  // it's corrupt by here...
  // dns_request *server_request = (dns_request *)request->user;

  // unsigned char buffer[DNC_BUF_SIZE];
  unsigned char *buffer = (unsigned char *)buf;

  // memset(&buffer, 0, DNC_BUF_SIZE);
  // ret = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr*)&addr,
  // &size);
  if(sz < 0)
  {
    llarp::LogWarn("Error Receiving DNS Client Response");
    request->resolved(request, nullptr);
    return;
  }

  // hexdump("received packet", &buffer, ret);

  llarp_ev_close_udp(udp);

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
  int rcode;
  int length;

  struct dns_query *dnsQuery = &request->query;

  rcode = (buffer[3] & 0x0F);

  // tempBuf[0] = buffer[4];
  // tempBuf[1] = buffer[5];
  // tempBuf[2] = '\0';

  // printf("%0x %0x %0x %0x\n", buffer[4], buffer[5], tempBuf[0], tempBuf[1]);

  // QDCOUNT = (uint16_t) strtol(tempBuf, NULL, 16);
  QDCOUNT = (uint16_t)buffer[4] * 0x100 + buffer[5];
  llarp::LogDebug("entries in question section: %u\n", QDCOUNT);
  ANCOUNT = (uint16_t)buffer[6] * 0x100 + buffer[7];
  llarp::LogDebug("records in answer section: %u\n", ANCOUNT);
  NSCOUNT = (uint16_t)buffer[8] * 0x100 + buffer[9];
  llarp::LogDebug("name server resource record count: %u\n", NSCOUNT);
  ARCOUNT = (uint16_t)buffer[10] * 0x100 + buffer[11];
  llarp::LogDebug("additional records count: %u\n", ARCOUNT);

  llarp::LogDebug("query type: %u\n", dnsQuery->reqType);
  QCLASS = (uint16_t)dnsQuery->request[dnsQuery->length - 2] * 0x100
      + dnsQuery->request[dnsQuery->length - 1];
  llarp::LogDebug("query class: %u\n", QCLASS);
  length = dnsQuery->length + 1;  // to skip 0xc00c
  ATYPE  = (uint16_t)buffer[length + 1] * 0x100 + buffer[length + 2];
  llarp::LogDebug("answer type: %u\n", ATYPE);
  ACLASS = (uint16_t)buffer[length + 3] * 0x100 + buffer[length + 4];
  llarp::LogDebug("answer class: %u\n", ACLASS);
  TTL = (uint32_t)buffer[length + 5] * 0x1000000 + buffer[length + 6] * 0x10000
      + buffer[length + 7] * 0x100 + buffer[length + 8];
  llarp::LogDebug("seconds to cache: %u\n", TTL);
  RDLENGTH = (uint16_t)buffer[length + 9] * 0x100 + buffer[length + 10];
  llarp::LogDebug("bytes in answer: %u\n", RDLENGTH);
  MSGID = (uint16_t)buffer[0] * 0x100 + buffer[1];
  llarp::LogDebug("answer msg id: %u\n", MSGID);

  if(rcode == 2)
  {
    llarp::LogWarn("nameserver %s returned SERVFAIL:\n", SERVER);
    llarp::LogWarn(
        "  the name server was unable to process this query due to a\n  "
        "problem with the name server.\n");
    request->resolved(request, nullptr);
    return;
  }
  else if(rcode == 3)
  {
    llarp::LogWarn("nameserver %s returned NXDOMAIN for %s:\n", SERVER,
                   dnsQuery->url);
    llarp::LogWarn(
        "  the domain name referenced in the query does not exist\n");
    request->resolved(request, nullptr);
    return;
  }

  int ip = 0;

  /* search for and print IPv4 addresses */
  if(dnsQuery->reqType == 0x01)
  {
    llarp::LogInfo("DNS server's answer is: (type#=%u):", ATYPE);
    printf("IPv4 address(es) for %s:\n", dnsQuery->url);
    for(unsigned int i = 0; i < sz; i++)
    {
      if(buffer[i] == 0xC0 && buffer[i + 3] == 0x01)
      {
        ip++;
        i += 12; /* ! += buf[i+1]; */
        llarp::LogDebug(" %u.%u.%u.%u\n", buffer[i], buffer[i + 1],
                        buffer[i + 2], buffer[i + 3]);
        struct sockaddr *g_addr = new sockaddr;
        g_addr->sa_family       = AF_INET;
        // g_addr->sa_len          = sizeof(in_addr);
        struct in_addr *addr = &((struct sockaddr_in *)g_addr)->sin_addr;
        unsigned char *ip;

        // have ip point to s_addr
        ip = (unsigned char *)&(addr->s_addr);

        ip[0] = buffer[i + 0];
        ip[1] = buffer[i + 1];
        ip[2] = buffer[i + 2];
        ip[3] = buffer[i + 3];

        // return g_addr;
        request->resolved(request, g_addr);
        return;
      }
    }

    if(!ip)
    {
      llarp::LogWarn("  No IPv4 address found in the DNS response!\n");
      request->resolved(request, nullptr);
      return;
    }
  }
}

void
build_dns_query(struct dns_query *dnsQuery)
{
  dnsQuery->length = 12;
  // dnsQuery->url     = sUrl;
  dnsQuery->reqType = 0x01;
  // dnsQuery.request  = { 0xDB, 0x42, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  // 0x00, 0x00, 0x00 };
  dnsQuery->request[0]  = 0xDB;
  dnsQuery->request[1]  = 0x42;
  dnsQuery->request[2]  = 0x01;
  dnsQuery->request[3]  = 0x00;
  dnsQuery->request[4]  = 0x00;
  dnsQuery->request[5]  = 0x01;
  dnsQuery->request[6]  = 0x00;
  dnsQuery->request[7]  = 0x00;
  dnsQuery->request[8]  = 0x00;
  dnsQuery->request[9]  = 0x00;
  dnsQuery->request[10] = 0x00;
  dnsQuery->request[11] = 0x00;

  char *word;
  llarp::LogDebug("Asking DNS server %s about %s\n", SERVER, dnsQuery->url);

  char *strTemp = strdup(dnsQuery->url);
  word          = strtok(strTemp, ".");
  while(word)
  {
    llarp::LogDebug("parsing hostname: \"%s\" is %zu characters\n", word,
                    strlen(word));
    dnsQuery->request[dnsQuery->length++] = strlen(word);
    for(unsigned int i = 0; i < strlen(word); i++)
    {
      dnsQuery->request[dnsQuery->length++] = word[i];
    }
    word = strtok(NULL, ".");
  }

  dnsQuery->request[dnsQuery->length++] = 0x00;  // End of the host name
  dnsQuery->request[dnsQuery->length++] =
      0x00;  // 0x0001 - Query is a Type A query (host address)
  dnsQuery->request[dnsQuery->length++] = dnsQuery->reqType;
  dnsQuery->request[dnsQuery->length++] =
      0x00;  // 0x0001 - Query is class IN (Internet address)
  dnsQuery->request[dnsQuery->length++] = 0x01;
}

bool
llarp_dns_resolve(dns_client_request *request)
{
  struct dns_query *dnsQuery = &request->query;
  build_dns_query(dnsQuery);
  struct sockaddr_in addr;
  // int socket;
  ssize_t ret;
  // socklen_t size;
  // unsigned char tempBuf[3];

  memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = inet_addr(SERVER);
  addr.sin_port        = htons(PORT);
  // size                 = sizeof(addr);

  llarp_udp_io *udp = (llarp_udp_io *)request->sock;
  // llarp::LogDebug("dns client set to use ");
  // XXX: udp user pointer should be set before binding to socket and once
  udp->user = request;

  // hexdump("sending packet", &dnsQuery.request, dnsQuery.length);
  // ret = sendto(sockfd, dnsQuery.request, dnsQuery.length, 0, (struct
  // sockaddr*)&addr, size);
  ret = llarp_ev_udp_sendto(udp, (sockaddr *)&addr, dnsQuery->request,
                            dnsQuery->length);
  if(ret < 0)
  {
    llarp::LogWarn("Error Sending Request");
    return false;
  }
  // dns_request *test = (dns_request *)request->user;

  // printf("Sent\n");
  llarp::LogInfo("Request sent, awaiting response");
  return true;
}

bool
llarp_resolve_host(struct llarp_ev_loop *netloop, const char *url,
                   resolve_dns_hook_func resolved, void *user)
{
  struct sockaddr_in s_addr;
  s_addr.sin_family      = AF_INET;
  s_addr.sin_addr.s_addr = inet_addr("0.0.0.0");

  llarp_udp_io *udp = new llarp_udp_io;
  udp->tick         = nullptr;
  udp->user         = nullptr;
  udp->impl         = nullptr;
  udp->parent       = netloop;  // add_udp will do this...
  // llarp::LogDebug("dns client set to use ");
  udp->recvfrom = &llarp_handle_dnsclient_recvfrom;

  if(llarp_ev_add_udp(netloop, udp, (sockaddr *)&s_addr) == -1)
  {
    llarp::LogError("failed to bind resolver to");
    return false;
  }

  dns_client_request *request = new dns_client_request;
  request->sock               = (void *)udp;
  request->user               = user;
  request->query.url          = strdup(url);
  request->resolved           = resolved;
  llarp_dns_resolve(request);
  return true;
}
