#ifndef LLARP_PATH_H
#define LLARP_PATH_H

#include <llarp/router_contact.h>

#define MAXHOPS (8)
#define DEFAULT_PATH_LIFETIME (10 * 60 * 1000)
#define PATH_BUILD_TIMEOUT (30 * 1000)

struct llarp_path_hop
{
  struct llarp_rc router;
  byte_t nextHop[PUBKEYSIZE];
  byte_t sessionkey[SHAREDKEYSIZE];
  byte_t pathid[PATHIDSIZE];
};

struct llarp_path_hops
{
  struct llarp_path_hop hops[MAXHOPS];
  size_t numHops;
};

void
llarp_path_hops_free(struct llarp_path_hops* hops);

#endif