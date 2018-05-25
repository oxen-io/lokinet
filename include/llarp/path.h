#ifndef LLARP_PATH_H
#define LLARP_PATH_H
#ifdef __cplusplus
extern "C" {
#endif

#include <llarp/types.h>

typedef uint64_t llarp_path_id_t;

struct llarp_transit_hop
{
  llarp_path_id_t id;
  llarp_sharedkey_t symkey;
  llarp_pubkey_t nextHop;
  llarp_pubkey_t prevHop;
  uint64_t started;
  uint64_t lifetime;
  llarp_proto_version_t version;
};

struct llarp_path_context;

#ifdef __cplusplus
}
#endif
#endif
