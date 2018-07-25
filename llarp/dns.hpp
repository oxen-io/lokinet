#ifndef LIBLLARP_DNS_HPP
#define LIBLLARP_DNS_HPP

#include <sys/types.h>  // for uint & ssize_t
#include <string>

// protocol parsing/writing structures & functions
struct dns_msg_header
{
  uint16_t id;
  uint8_t qr : 1;
  uint8_t opcode : 4;
  uint8_t aa : 1;
  uint8_t tc : 1;
  uint8_t rd : 1;

  uint8_t ra : 1;
  uint8_t z : 1;
  uint8_t ad : 1;
  uint8_t cd : 1;
  uint8_t rcode : 4;

  uint16_t qdCount;
  uint16_t anCount;
  uint16_t nsCount;
  uint16_t arCount;
};

struct dns_msg_question
{
  std::string name;
  uint16_t type;
  uint16_t qClass;
};

struct dns_msg_answer
{
  std::string name;
  uint16_t type;
  uint16_t aClass;
  uint32_t ttl;
  uint16_t rdLen;
  uint8_t *rData;
};

uint16_t
get16bits(const char *&buffer) throw();

uint32_t
get32bits(const char *&buffer) throw();

dns_msg_header *
decode_hdr(const char *buffer);

dns_msg_question *
decode_question(const char *buffer);

dns_msg_answer *
decode_answer(const char *buffer);

void
put16bits(char *&buffer, uint16_t value) throw();

void
put32bits(char *&buffer, uint32_t value) throw();

void
code_domain(char *&buffer, const std::string &domain) throw();

void
llarp_handle_dns_recvfrom(struct llarp_udp_io *udp,
                          const struct sockaddr *saddr, const void *buf,
                          ssize_t sz);

#endif
