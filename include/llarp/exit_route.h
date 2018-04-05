#ifndef LLARP_XR_H
#define LLARP_XR_H
#include <llarp/buffer.h>
#include <llarp/net.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

struct llarp_xr {
  struct in6_addr gateway;
  struct in6_addr netmask;
  struct in6_addr source;
  uint64_t lifetime;
};

bool llarp_xr_bencode(struct llarp_xr* xr, llarp_buffer_t* buff);
bool llarp_xr_bdecode(struct llarp_xr* xr, llarp_buffer_t* buff);

#ifdef __cplusplus
}
#endif
#endif
