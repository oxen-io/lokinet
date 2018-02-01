#include "net.hpp"
#include "str.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>

namespace llarp {
namespace net {
bool GetIfAddr(const std::string& ifname, int af, sockaddr* addr) {
  ifaddrs* ifa = nullptr;
  bool found = false;
  socklen_t sl = sizeof(sockaddr_in6);
  if (af == AF_INET) sl = sizeof(sockaddr_in);

  if (getifaddrs(&ifa) == -1) return false;
  ifaddrs* i = ifa;
  while (i) {
    if (llarp::StrEq(i->ifa_name, ifname.c_str()) && i->ifa_addr &&
        i->ifa_addr->sa_family == af) {
      memcpy(addr, i->ifa_addr, sl);
      found = true;
      break;
    }
    i = i->ifa_next;
  }
  if (ifa) freeifaddrs(ifa);
  return found;
}
}  // namespace net
}  // namespace llarp
