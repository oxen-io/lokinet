#pragma once

#include <llarp/crypto/crypto.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/util/buffer.hpp>

/**
 * dht.h
 *
 * DHT functions
 */

struct llarp_dht_context;

namespace llarp
{
  struct AbstractRouter;
}

/// allocator
struct llarp_dht_context*
llarp_dht_context_new(llarp::AbstractRouter* parent);

/// deallocator
void
llarp_dht_context_free(struct llarp_dht_context* dht);

/// start dht context with our location in keyspace
void
llarp_dht_context_start(struct llarp_dht_context* ctx, const byte_t* key);

// remove this? dns needs it atm
struct llarp_router_lookup_job;

using llarp_router_lookup_handler = void (*)(struct llarp_router_lookup_job*);

struct llarp_router_lookup_job
{
  /// can be anything but usually a class context for hook
  void* user;
  llarp_router_lookup_handler hook;
  struct llarp_dht_context* dht;
  llarp::PubKey target;
  bool found;
  // make sure you initialize addr and exits
  llarp::RouterContact result;
  bool iterative;
};
// end dns requirement

/// start allowing dht participation on a context
void
llarp_dht_allow_transit(struct llarp_dht_context* ctx);

/// remove router from tracked dht peer list
/// internal function do not use
void
__llarp_dht_remove_peer(struct llarp_dht_context* ctx, const byte_t* id);

void
llarp_dht_lookup_router(struct llarp_dht_context* ctx, struct llarp_router_lookup_job* job);
