#ifndef LLARP_PATH_H
#define LLARP_PATH_H

#include <llarp/router_contact.h>

#define MAXHOPS (8)
#ifdef __cplusplus
extern "C"
{
#endif

  struct llarp_path_hops
  {
    struct llarp_rc routers[MAXHOPS];
    size_t numHops;
  };

#ifdef __cplusplus
}
#endif
#endif