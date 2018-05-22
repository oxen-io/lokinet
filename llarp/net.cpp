#include "net.hpp"
#include "str.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <cstdio>

bool
operator==(const sockaddr& a, const sockaddr& b)
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

bool
operator<(const sockaddr_in6& a, const sockaddr_in6& b)
{
  return memcmp(&a, &b, sizeof(sockaddr_in6)) < 0;
}

bool
operator<(const in6_addr& a, const in6_addr& b)
{
  return memcmp(&a, &b, sizeof(in6_addr)) < 0;
}

extern "C" {

bool
llarp_getifaddr(const char* ifname, int af, struct sockaddr* addr)
{
  ifaddrs* ifa = nullptr;
  bool found   = false;
  socklen_t sl = sizeof(sockaddr_in6);
  if(af == AF_INET)
    sl = sizeof(sockaddr_in);
  if(af == AF_PACKET)
    sl = sizeof(sockaddr_ll);

  if(getifaddrs(&ifa) == -1)
    return false;
  ifaddrs* i = ifa;
  while(i)
  {
    if(i->ifa_addr)
    {
      if(llarp::StrEq(i->ifa_name, ifname) && i->ifa_addr->sa_family == af)
      {
        memcpy(addr, i->ifa_addr, sl);
        addr->sa_family = af;
        found           = true;
        break;
      }
    }
    i = i->ifa_next;
  }
  if(ifa)
    freeifaddrs(ifa);
  return found;
}
}
