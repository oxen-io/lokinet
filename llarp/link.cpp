#include "link.hpp"
#include "net.hpp"
#include <llarp/time.h>
#include <cstring>
#include <arpa/inet.h>
#include <uv.h>

bool operator<(const sockaddr_in6 addr0, const sockaddr_in6 addr1) {
  return memcmp(addr0.sin6_addr.s6_addr, addr1.sin6_addr.s6_addr,
                sizeof(addr0.sin6_addr)) &&
         addr0.sin6_port < addr1.sin6_port;
}


namespace llarp
{
}

extern "C" {
struct llarp_link *llarp_link_alloc() {
  return new llarp_link;
}

void llarp_link_free(struct llarp_link **l) {
  if (*l) delete *l;
  *l = nullptr;
}

struct llarp_udp_listener *llarp_link_udp_listener(struct llarp_link *l) {
  return &l->listener;
}

  bool llarp_link_configure_addr(struct llarp_link *link, const char *ifname, int af, uint16_t port) {
    af = AF_INET6;
    link->af = af;
    if(llarp::net::GetIfAddr(ifname, link->af, (sockaddr*)&link->localaddr))
    {
      link->localaddr.sin6_family = af;
      link->localaddr.sin6_port = htons(port);
      link->listener.addr = &link->localaddr;
      char buff[128] = {0};
      uv_ip6_name(&link->localaddr, buff, sizeof(buff));
      printf("link %s configured with address %s\n", ifname, buff);
      return true;
    }
    return false;
  }

void llarp_link_stop(struct llarp_link *link) {
  llarp_ev_close_udp_listener(&link->listener);
}
}
