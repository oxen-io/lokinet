#ifndef LLARP_XI_H
#define LLARP_XI_H
#include <llarp/buffer.h>
#include <llarp/net.h>

#ifdef __cplusplus
extern "C" {
#endif
  
  struct llarp_xi
  {
    in6_addr address;
    in6_addr netmask;
  };

  
  bool llarp_xi_bdecode(struct llarp_xi * xi, llarp_buffer_t * buf);
  bool llarp_xi_bencode(struct llarp_xi * xi, llarp_buffer_t * buf);
  
  struct llarp_xi_list;
  
#ifdef __cplusplus
}
#endif
#endif
