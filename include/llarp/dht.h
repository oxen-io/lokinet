#ifndef LLARP_DHT_H_
#define LLARP_DHT_H_
#ifdef __cplusplus
extern "C" {
#endif

struct llarp_dht_context;

struct llarp_dht_context*
llarp_dht_context_new();
void
llarp_dht_context_free(struct llarp_dht_context* dht);

#ifdef __cplusplus
}
#endif
#endif
