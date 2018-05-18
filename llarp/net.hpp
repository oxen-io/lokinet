#ifndef LLARP_NET_HPP
#define LLARP_NET_HPP
#include <llarp/net.h>

bool operator == (const sockaddr& a, const sockaddr& b)
{
  socklen_t sz = sizeof(a.sa_data);
  switch(a.sa_family)
  {
  case AF_INET:
    sz = sizeof(sockaddr_in);
    break;
  case AF_INET6:
    sz = sizeof(sockaddr_in6);
    break;
  case AF_PACKET:
    sz = sizeof(sockaddr_ll);
    break;
  default:
    break;
  }
  return a.sa_family == b.sa_family && memcmp(a.sa_data, b.sa_data, sz) == 0;
}

#endif
