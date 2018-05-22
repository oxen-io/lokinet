#ifndef LLARP_PATH_H
#define LLARP_PATH_H

#include <llarp/types.h>

typedef uint64_t llarp_path_id_t;

struct llarp_transit_hop
{
  llarp_path_id_t id;
  llarp_sharedkey_t symkey;
  llarp_pubkey_t nextHop;
  uint64_t started;
  uint64_t lifetime;
  llarp_version_t version;
};

#endif
