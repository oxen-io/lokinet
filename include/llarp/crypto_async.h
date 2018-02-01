#ifndef LLARP_CRYPTO_ASYNC_H_
#define LLARP_CRYPTO_ASYNC_H_
#include <llarp/crypto.h>
#include <llarp/ev.h>
#include <llarp/threadpool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_async_dh;

struct llarp_async_dh *llarp_async_dh_new(struct llarp_crypto *crypto,
                                          struct llarp_ev_loop *ev,
                                          struct llarp_threadpool *tp);
void llarp_async_dh_free(struct llarp_async_dh **dh);

struct llarp_dh_result;

typedef void (*llarp_dh_complete_hook)(struct llarp_dh_result *);

struct llarp_dh_internal;

struct llarp_dh_result {
  struct llarp_dh_internal *impl;
  llarp_sharedkey_t result;
  void *user;
  llarp_dh_complete_hook hook;
};

void llarp_async_client_dh(struct llarp_async_dh *dh, llarp_seckey_t ourkey,
                           llarp_pubkey_t theirkey,
                           llarp_tunnel_nounce_t nounce,
                           llarp_dh_complete_hook result, void *user);
void llarp_async_server_dh(struct llarp_async_dh *dh, llarp_seckey_t ourkey,
                           llarp_pubkey_t theirkey,
                           llarp_tunnel_nounce_t nounce,
                           llarp_dh_complete_hook result, void *user);

#ifdef __cplusplus
}
#endif
#endif
