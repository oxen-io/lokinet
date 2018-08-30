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

/// start allowing dht participation on a context
void
llarp_dht_allow_transit(struct llarp_dht_context* ctx);

/// remove router from tracked dht peer list
/// internal function do not use
void
__llarp_dht_remove_peer(struct llarp_dht_context* ctx, const byte_t* id);

#endif
