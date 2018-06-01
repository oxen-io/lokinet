#ifndef LLARP_DHT_H_
#define LLARP_DHT_H_

#include <llarp/buffer.h>

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
llarp_dht_context_new();

/// deallocator
void
llarp_dht_context_free(struct llarp_dht_context* dht);

struct llarp_dht_msg;

/// handler function
/// f(outmsg, inmsg)
/// returns true if outmsg has been filled otherwise returns false
typedef bool (*llarp_dht_msg_handler)(struct llarp_dht_msg*,
                                      struct llarp_dht_msg*);

void
llarp_dht_context_set_our_key(struct llarp_dht_context* ctx, const byte_t* key);

// override dht message handler with custom handler
void
llarp_dht_set_msg_handler(struct llarp_dht_context* ctx,
                          llarp_dht_msg_handler func);

#ifdef __cplusplus
}
#endif
#endif
