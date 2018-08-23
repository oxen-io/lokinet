#ifndef LLARP_DHT_H_
#define LLARP_DHT_H_

#include <llarp/buffer.h>
#include <llarp/router.h>

/**
 * dht.h
 *
 * DHT functions
 */

struct llarp_dht_context;

/// allocator
struct llarp_dht_context*
llarp_dht_context_new(struct llarp_router* parent);

/// deallocator
void
llarp_dht_context_free(struct llarp_dht_context* dht);

/// start dht context with our location in keyspace
void
llarp_dht_context_start(struct llarp_dht_context* ctx, const byte_t* key);

struct llarp_router_lookup_job;

typedef void (*llarp_router_lookup_handler)(struct llarp_router_lookup_job*);

struct llarp_router_lookup_job
{
  /// can be anything but usually a class context for hook
  void* user;
  llarp_router_lookup_handler hook;
  struct llarp_dht_context* dht;
  byte_t target[PUBKEYSIZE];
  bool found;
  // make sure you initialize addr and exits
  struct llarp_rc result;
  bool iterative;
};

/// start allowing dht participation on a context
void
llarp_dht_allow_transit(struct llarp_dht_context* ctx);

/// put router as a dht peer
/// internal function do not use
void
__llarp_dht_put_peer(struct llarp_dht_context* ctx, struct llarp_rc* rc);

/// remove router from tracked dht peer list
/// internal function do not use
void
__llarp_dht_remove_peer(struct llarp_dht_context* ctx, const byte_t* id);

void
llarp_dht_lookup_router(struct llarp_dht_context* ctx,
                        struct llarp_router_lookup_job* job);

#endif
