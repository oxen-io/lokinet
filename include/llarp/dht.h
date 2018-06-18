#ifndef LLARP_DHT_H_
#define LLARP_DHT_H_

#include <llarp/buffer.h>
#include <llarp/router.h>

/**
 * dht.h
 *
 * DHT functions
 */

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_dht_context;

/// allocator
struct llarp_dht_context*
llarp_dht_context_new(struct llarp_router* parent);

/// deallocator
void
llarp_dht_context_free(struct llarp_dht_context* dht);

struct llarp_dht_msg;

/// handler function
/// f(outmsg, inmsg)
/// returns true if outmsg has been filled otherwise returns false
typedef bool (*llarp_dht_msg_handler)(struct llarp_dht_msg*,
                                      struct llarp_dht_msg*);

/// start dht context with our location in keyspace
void
llarp_dht_context_start(struct llarp_dht_context* ctx, const byte_t* key);

// override dht message handler with custom handler
void
llarp_dht_set_msg_handler(struct llarp_dht_context* ctx,
                          llarp_dht_msg_handler func);

struct llarp_router_lookup_job;

typedef void (*llarp_router_lookup_handler)(struct llarp_router_lookup_job*);

struct llarp_router_lookup_job
{
  void* user;
  llarp_router_lookup_handler hook;
  struct llarp_dht_context* dht;
  byte_t target[PUBKEYSIZE];
  bool found;
  struct llarp_rc result;
};

/// start allowing dht participation on a context
void
llarp_dht_allow_transit(struct llarp_dht_context* ctx);

/// put router as a dht peer
void
llarp_dht_put_peer(struct llarp_dht_context* ctx, struct llarp_rc* rc);

/// remove router from tracked dht peer list
void
llarp_dht_remove_peer(struct llarp_dht_context* ctx, const byte_t* id);

void
llarp_dht_lookup_router(struct llarp_dht_context* ctx,
                        struct llarp_router_lookup_job* job);

#ifdef __cplusplus
}
#endif
#endif
