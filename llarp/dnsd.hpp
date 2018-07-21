#ifndef LIBLLARP_DNSD_HPP
#define LIBLLARP_DNSD_HPP

#include "dnsc.hpp"

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

dns_msg_header *
decode_hdr(const char *buffer);
dns_msg_question *
decode_question(const char *buffer);
dns_msg_answer *
decode_answer(const char *buffer);

#endif
