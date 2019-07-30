#ifndef LLARP_DNS_HPP
#define LLARP_DNS_HPP

#include <dns.h>
#include <sys/types.h>  // for uint & ssize_t

#ifndef _WIN32
#include <sys/socket.h>
#endif

#include <dns/rectypes.hpp>
#include <net/net.hpp>  // for llarp::Addr , llarp::huint32_t

#include <map>
#include <string>
#include <vector>

#define LLARP_DNS_RECTYPE_A 1
#define LLARP_DNS_RECTYPE_NS 2
#define LLARP_DNS_RECTYPE_CNAME 5
#define LLARP_DNS_RECTYPE_SOA 6
#define LLARP_DNS_RECTYPE_PTR 12
#define LLARP_DNS_RECTYPE_MX 15
#define LLARP_DNS_RECTYPE_TXT 16

struct dnsc_answer_request;
struct dnsd_context;

// dnsc can work over any UDP socket
// however we can't ignore udp->user
// we need to be able to reference the request (being a request or response)
// bottom line is we can't use udp->user
// so we'll need to track all incoming and outgoing requests
struct dns_tracker
{
  // uint c_responses;
  uint32_t c_requests;
  // request has to be a pointer
  std::unordered_map< uint32_t, std::unique_ptr< dnsc_answer_request > >
      client_request;
  // FIXME: support multiple dns server contexts
  dnsd_context *dnsd;
  // rn we need 1 tracker per DNSd and each DNSd needs it's own IP
  // actually we can bind once and use the tracker to sort
  // but no way to tell what DNSd they want...
  // std::map< llarp::Addr, std::unique_ptr< dnsc_answer_request > > dnsds;
  // std::map< uint, dnsd_question_request * > daemon_request;
};

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

  uint16_t
  fields() const
  {
    return (qr << 15) | (opcode << 14) | (aa << 10) | (tc << 9)
        | (rd << 8) << (ra << 7) | (z << 6) | rcode;
  }

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
  std::vector< byte_t > rData;
  std::unique_ptr< llarp::dns::record > record;
};

struct dns_packet
{
  struct dns_msg_header header;
  std::vector< std::unique_ptr< dns_msg_question > > questions;
  std::vector< std::unique_ptr< dns_msg_answer > > answers;
  std::vector< std::unique_ptr< dns_msg_answer > > auth_rrs;
  std::vector< std::unique_ptr< dns_msg_answer > > additional_rrs;
};

std::vector< byte_t >
packet2bytes(dns_packet &in);

std::string
getDNSstring(const char *const buffer, uint32_t *pos);

void
code_domain(char *&buffer, const std::string &domain) noexcept;

void
vcode_domain(std::vector< byte_t > &bytes, const std::string &domain) noexcept;

void
vput16bits(std::vector< byte_t > &bytes, uint16_t value) noexcept;

void
vput32bits(std::vector< byte_t > &bytes, uint32_t value) noexcept;

extern "C"
{
  uint16_t
  get16bits(const char *&buffer) noexcept;

  uint32_t
  get32bits(const char *&buffer) noexcept;

  bool
  decode_hdr(llarp_buffer_t *buffer, dns_msg_header *hdr);

  dns_msg_question *
  decode_question(const char *buffer, uint32_t *pos);

  dns_msg_answer *
  decode_answer(const char *const buffer, uint32_t *pos);

  void
  put16bits(char *&buffer, uint16_t value) noexcept;

  void
  put32bits(char *&buffer, uint32_t value) noexcept;

  void
  llarp_handle_dns_recvfrom(struct llarp_udp_io *udp,
                            const struct sockaddr *addr, ManagedBuffer buf);
}
#endif
