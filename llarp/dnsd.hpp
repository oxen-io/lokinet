#ifndef LIBLLARP_DNSD_HPP
#define LIBLLARP_DNSD_HPP

#include <string>
#include "dnsc.hpp"

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

typedef ssize_t (*sendto_dns_hook_func)(void *sock, const struct sockaddr *from,
                                        const void *buffer, size_t length);

struct dns_request
{
  /// sock type
  void *user;
  /// request id
  int id;
  std::string m_qName;
  uint m_qType;
  uint m_qClass;
  struct sockaddr *from;
  sendto_dns_hook_func hook;  // sendto hook tbh
};

#endif
