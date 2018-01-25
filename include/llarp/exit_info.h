#ifndef LLARP_XI_H
#define LLARP_XI_H
#include <llarp/net.h>

#ifdef __cplusplus
extern "C" {
#endif
  
  struct llarp_exit_info
  {
    in6_addr address;
    in6_addr netmask;
  };

  struct llarp_exit_info_list;
  
#ifdef __cplusplus
}
#endif
#endif
