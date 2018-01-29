#include "link.hpp"
#include <cstring>
#include <llarp/time.h>

bool operator < (const sockaddr_in6 addr0, const sockaddr_in6 addr1)
{
  return memcmp(addr0.sin6_addr.s6_addr, addr1.sin6_addr.s6_addr, sizeof(addr0.sin6_addr)) && addr0.sin6_port < addr1.sin6_port;
}

extern "C"{
  struct llarp_link * llarp_link_alloc()
  {
    return new llarp_link;
  }

  void llarp_link_free(struct llarp_link ** l)
  {
    if(*l) delete *l;
    *l =nullptr;
  }

  struct llarp_udp_listener * llarp_link_udp_listener(struct llarp_link *l)
  {
    return &l->listener;
  }

  bool llarp_link_configure(struct llarp_link * link, const char * ifname, int af)
  {
    return false;
  }

  void llarp_link_stop(struct llarp_link * link)
  {
    llarp_ev_close_udp_listener(&link->listener);
  }
}
