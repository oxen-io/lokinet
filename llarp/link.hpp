#ifndef LLARP_LINK_HPP
#define LLARP_LINK_HPP
#include <cstdint>
#include <functional>
#include <llarp/crypto.h>
#include <map>
#include <memory>
#include <netinet/in.h>

#include "mem.hpp"
#include <llarp/ev.h>
#include <llarp/router_contact.h>

struct llarp_link {
  static void *operator new(size_t sz) {
    return llarp_g_mem.alloc(sz, llarp::alignment<llarp_link>());
  }

  static void operator delete(void *ptr) { llarp_g_mem.free(ptr); }

  struct sockaddr_in6 localaddr;
  int af;
  llarp_udp_listener listener;
};

#endif
