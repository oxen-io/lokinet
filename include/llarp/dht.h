#ifndef LLARP_DHT_H_
#define LLARP_DHT_H_

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

#ifdef __cplusplus
}
#endif
#endif
