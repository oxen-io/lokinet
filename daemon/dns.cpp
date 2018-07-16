
#include <getopt.h>
#include <signal.h>
#include <stdio.h> /* fprintf, printf */
#include <unistd.h>

#include <netdb.h>  /* getaddrinfo, getnameinfo */
#include <stdlib.h> /* exit */
#include <string.h> /* memset */
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

#include "logger.hpp"
#include "net.hpp"

bool done = false;

void
handle_signal(int sig)
{
  printf("got SIGINT\n");
  done = true;
}

#define BUF_SIZE 512

struct query
{
  uint16_t length;
  char *url;
  unsigned char request[BUF_SIZE];
  uint16_t reqType;
};

#define SERVER "8.8.8.8"
#define PORT 53

struct sockaddr *
resolveHost(char *url)
{
  struct query dnsQuery;
  dnsQuery.length  = 12;
  dnsQuery.url     = url;
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
  dnsQuery.url     = strdup(url);
  dnsQuery.reqType = 0x01;

  word = strtok(url, ".");
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
  unsigned char buffer[BUF_SIZE];
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

  memset(&buffer, 0, BUF_SIZE);
  ret = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)&addr, &size);
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
        // g_addr->sa_len = sizeof(in_addr);
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

int
get16bits(const char *&buffer) throw()
{
  int value = static_cast< unsigned char >(buffer[0]);
  value     = value << 8;
  value += static_cast< unsigned char >(buffer[1]);
  buffer += 2;
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
  buffer[1] = (value & 0xFF0000) >> 16;
  buffer[2] = (value & 0xFF00) >> 16;
  buffer[3] = (value & 0xFF) >> 16;
  buffer += 4;
}

struct dns_msg
{
  uint id;
  uint qr;
  uint opcode;
  uint aa;
  uint tc;
  uint rd;
  uint ra;
  uint rcode;

  uint qdCount;
  uint anCount;
  uint nsCount;
  uint arCount;
};

dns_msg *
decode_hdr(const char *buffer)
{
  dns_msg *hdr = new dns_msg;
  hdr->id      = get16bits(buffer);
  uint fields  = get16bits(buffer);
  hdr->qr      = fields & 0x8000;
  hdr->opcode  = fields & 0x7800;
  hdr->aa      = fields & 0x0400;
  hdr->tc      = fields & 0x0200;
  hdr->rd      = fields & 0x0100;
  hdr->ra      = fields & 0x8000;

  hdr->qdCount = get16bits(buffer);
  hdr->anCount = get16bits(buffer);
  hdr->nsCount = get16bits(buffer);
  hdr->arCount = get16bits(buffer);
  return hdr;
}

void
code_domain(char *&buffer, const std::string &domain) throw()
{
  std::string::size_type start(0), end;  // indexes
  llarp::LogInfo("domain [", domain, "]");
  while((end = domain.find('.', start)) != std::string::npos)
  {
    *buffer++ = end - start;  // label length octet
    for(std::string::size_type i = start; i < end; i++)
    {
      *buffer++ = domain[i];  // label octets
      llarp::LogInfo("Writing ", domain[i], " at ", i);
    }
    start = end + 1;  // Skip '.'
  }

  llarp::LogInfo("start ", start, " domain size ", domain.size());

  *buffer++ = domain.size() - start;  // last label length octet
  for(size_t i = start; i < domain.size(); i++)
  {
    *buffer++ = domain[i];  // last label octets
    llarp::LogInfo("Writing ", domain[i], " at ", i);
  }

  *buffer++ = 0;
}

struct dns_response
{
  std::string m_name;
  uint m_type;
  uint m_class;
  unsigned long m_ttl;
  uint m_rdLength;
  std::string m_rdata;
};

int
main(int argc, char *argv[])
{
  int code = 1;
  llarp::LogInfo("Starting up server");

  struct sockaddr_in m_address;
  int m_sockfd;

  m_sockfd                  = socket(AF_INET, SOCK_DGRAM, 0);
  m_address.sin_family      = AF_INET;
  m_address.sin_addr.s_addr = INADDR_ANY;
  m_address.sin_port        = htons(1053);
  int rbind =
      bind(m_sockfd, (struct sockaddr *)&m_address, sizeof(struct sockaddr_in));

  if(rbind != 0)
  {
    llarp::LogError("Could not bind: ", strerror(errno));
    return 0;
  }

  const size_t BUFFER_SIZE = 1024;
  const size_t HDR_OFFSET  = 12;
  char buffer[BUFFER_SIZE];  // 1024 is buffer size
  struct sockaddr_in clientAddress;
  socklen_t addrLen = sizeof(struct sockaddr_in);

  signal(SIGINT, handle_signal);
  while(!done)
  {
    // sigint quits after next packet
    int nbytes = recvfrom(m_sockfd, buffer, BUFFER_SIZE, 0,
                          (struct sockaddr *)&clientAddress, &addrLen);
    llarp::LogInfo("Received Bytes ", nbytes);

    const char *p_buffer = buffer;
    dns_msg *msg         = decode_hdr(p_buffer);
    // llarp::LogInfo("DNS_MSG size", sizeof(dns_msg));
    p_buffer += HDR_OFFSET;
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
    uint m_qType  = get16bits(p_buffer);
    uint m_qClass = get16bits(p_buffer);
    llarp::LogInfo("qName  ", m_qName);
    llarp::LogInfo("qType  ", m_qType);
    llarp::LogInfo("qClass ", m_qClass);

    std::string copy(m_qName);
    sockaddr *hostRes = resolveHost((char *)copy.c_str());
    if(!hostRes)
    {
      exit(0);
    }
    llarp::Addr anIp(*hostRes);
    llarp::LogInfo("DNS got ", anIp);

    /*
    m_query.decode(buffer, nbytes);
    m_query.asString();

    m_resolver.process(m_query, m_response);

    m_response.asString();
    */
    memset(buffer, 0, BUFFER_SIZE);
    char *write_buffer = buffer;
    char *bufferBegin  = buffer;
    // build header
    put16bits(write_buffer, msg->id);
    int fields = (1 << 15);  // QR => message type, 1 = response
    fields += (0 << 14);     // I think opcode is always 0
    fields += 3;             // response code (3 => not found, 0 = Ok)
    put16bits(write_buffer, fields);

    put16bits(write_buffer, 1);  // QD (number of questions)
    put16bits(write_buffer, 1);  // AN (number of answers)
    put16bits(write_buffer, 0);  // NS (number of auth RRs)
    put16bits(write_buffer, 0);  // AR (number of Additional RRs)
    write_buffer += HDR_OFFSET;

    // code question
    llarp::LogInfo("qName2 ", m_qName);

    // 0123456789
    // 3bob3com1\0
    code_domain(write_buffer, m_qName);

    put16bits(write_buffer, m_qType);
    put16bits(write_buffer, m_qClass);

    // code answer
    std::string resp_str(inet_ntoa(*anIp.addr4()));
    code_domain(write_buffer, "");
    put16bits(write_buffer, m_qType);
    put16bits(write_buffer, m_qClass);
    put32bits(write_buffer, 0);  // ttl
    put16bits(write_buffer, 1);  // rdLength
    uint out_bytes = write_buffer - bufferBegin;
    llarp::LogInfo("Sending ", out_bytes, " bytes");

    // nbytes = m_response.code(buffer);

    sendto(m_sockfd, buffer, out_bytes, 0, (struct sockaddr *)&clientAddress,
           addrLen);
  }
  /*
  std::string host("www.google.com");
  sockaddr *hostRes = resolveHost((char *)host.c_str());
  if (!hostRes)
  {
    exit(0);
  }
  llarp::Addr anIp(*hostRes);
  llarp::LogInfo("DNS got ", anIp);
  */

  return code;
}
