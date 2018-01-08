#ifndef SARP_XI_H
#define SARP_XI_H
#include <sarp/net.h>

#ifdef __cplusplus
extern "C" {
#endif
  
  struct sarp_exit_info
  {
    in6_addr address;
    in6_addr netmask;
  };

  struct sarp_exit_info_list;
  
#ifdef __cplusplus
}
#endif
#endif
