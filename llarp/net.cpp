#include <llarp/net.h>
#include "str.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <cstdio>

extern "C" {
  
bool llarp_getifaddr(const char * ifname, int af, struct sockaddr* addr) {
  ifaddrs* ifa = nullptr;
  bool found = false;
  socklen_t sl = sizeof(sockaddr_in6);
  if (af == AF_INET) sl = sizeof(sockaddr_in);
  if (af == AF_PACKET) sl = sizeof(sockaddr_ll);

  if (getifaddrs(&ifa) == -1) return false;
  ifaddrs* i = ifa;
  while (i) {
    if (i->ifa_addr)
    {
      if (llarp::StrEq(i->ifa_name, ifname) && i->ifa_addr->sa_family == af) {
        memcpy(addr, i->ifa_addr, sl);
        addr->sa_family = af;
        found = true;
        break;
      }
    }
    i = i->ifa_next;
  }
  if (ifa) freeifaddrs(ifa);
  return found;
}
  
}
