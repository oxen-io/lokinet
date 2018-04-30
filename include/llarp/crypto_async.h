#ifndef LLARP_CRYPTO_ASYNC_H_
#define LLARP_CRYPTO_ASYNC_H_
#include <llarp/crypto.h>
#include <llarp/ev.h>
#include <llarp/threadpool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_async_dh;

struct llarp_async_dh *llarp_async_dh_new(llarp_seckey_t ourkey,
                                          struct llarp_crypto *crypto,
                                          struct llarp_threadpool *handler,
                                          struct llarp_threadpool *worker);
void llarp_async_dh_free(struct llarp_async_dh **dh);

struct llarp_dh_result;

typedef void (*llarp_dh_complete_hook)(struct llarp_dh_result *);

struct llarp_dh_result {
  llarp_sharedkey_t sharedkey;
  void *user;
  llarp_dh_complete_hook hook;
};

void llarp_async_client_dh(struct llarp_async_dh *dh, llarp_pubkey_t theirkey,
                           llarp_tunnel_nounce_t nounce,
                           llarp_dh_complete_hook result, void *user);

void llarp_async_server_dh(struct llarp_async_dh *dh, llarp_pubkey_t theirkey,
                           llarp_tunnel_nounce_t nounce,
                           llarp_dh_complete_hook result, void *user);

struct llarp_async_cipher;
struct llarp_cipher_result;

typedef void (*llarp_cipher_complete_hook)(struct llarp_cipher_result *);

struct llarp_cipher_result {
  llarp_buffer_t buff;
  void *user;
  llarp_cipher_complete_hook hook;
};

struct llarp_async_cipher *llarp_async_cipher_new(llarp_sharedkey_t key,
                                                  struct llarp_crypto *crypto,
                                                  struct llarp_threadpool *result,
                                                  struct llarp_threadpool *worker);

void llarp_async_cipher_free(struct llarp_async_cipher **c);

void llarp_async_cipher_queue_op(struct llarp_async_cipher *c,
                                 llarp_buffer_t *buff, llarp_nounce_t n,
                                 llarp_cipher_complete_hook h, void *user);

#ifdef __cplusplus
}
#endif
#endif
