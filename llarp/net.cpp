#include "net.hpp"
#include "str.hpp"
#ifdef ANDROID
#include "android/ifaddrs.h"
#else
#include <ifaddrs.h>
#endif

#include <arpa/inet.h>

#include <net/if.h>
#include <cstdio>
#include "logger.hpp"

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

bool
llarp_getifaddr(const char* ifname, int af, struct sockaddr* addr)
{
  ifaddrs* ifa = nullptr;
  bool found   = false;
  socklen_t sl = sizeof(sockaddr_in6);
  if(af == AF_INET)
    sl = sizeof(sockaddr_in);

  if(getifaddrs(&ifa) == -1)
    return false;
  ifaddrs* i = ifa;
  while(i)
  {
    if(i->ifa_addr)
    {
      // llarp::LogInfo(__FILE__, "scanning ", i->ifa_name, " af: ",
      // std::to_string(i->ifa_addr->sa_family));
      if(llarp::StrEq(i->ifa_name, ifname) && i->ifa_addr->sa_family == af)
      {
        llarp::Addr a(*i->ifa_addr);
        if(!a.isPrivate())
        {
          // llarp::LogInfo(__FILE__, "found ", ifname, " af: ", af);
          memcpy(addr, i->ifa_addr, sl);
          if(af == AF_INET6)
          {
            // set scope id
            sockaddr_in6* ip6addr  = (sockaddr_in6*)addr;
            ip6addr->sin6_scope_id = if_nametoindex(ifname);
            ip6addr->sin6_flowinfo = 0;
          }
          found = true;
          break;
        }
      }
    }
    i = i->ifa_next;
  }
  if(ifa)
    freeifaddrs(ifa);
  return found;
}
