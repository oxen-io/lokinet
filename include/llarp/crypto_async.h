#ifndef LLARP_CRYPTO_ASYNC_H_
#define LLARP_CRYPTO_ASYNC_H_
#include <llarp/crypto.h>
#include <llarp/ev.h>
#include <llarp/threadpool.h>
#include <llarp/logic.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_async_dh;

struct llarp_async_dh *llarp_async_dh_new(
  struct llarp_alloc * mem,
  llarp_seckey_t ourkey,
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


struct llarp_async_iwp;

struct llarp_async_iwp * llarp_async_iwp_new(struct llarp_alloc * mem,
                                             struct llarp_crypto * crypto,
                                             struct llarp_logic * logic,
                                             struct llarp_threadpool * worker);

  
struct iwp_async_keygen;

typedef void (*iwp_keygen_hook)(struct iwp_async_keygen *);

struct iwp_async_keygen
{
  struct llarp_async_iwp * iwp;
  void * user;
  uint8_t * keybuf;
  iwp_keygen_hook hook;
};

void iwp_call_async_keygen(struct llarp_async_iwp * iwp, struct iwp_async_keygen * keygen);

  

struct iwp_async_intro;
  
typedef void (*iwp_intro_gen_hook)(struct iwp_async_intro *);
  
struct iwp_async_intro
{
  struct llarp_async_iwp * iwp;
  void * user;
  uint8_t * buf;
  size_t sz;
  /** nonce paramter */
  uint8_t * nonce;
  /** remote public key */
  uint8_t * remote_pubkey;
  /** local private key */
  uint8_t * secretkey;
  /** resulting shared key */
  uint8_t * sharedkey;
  /** callback */
  iwp_intro_gen_hook hook;
};


void iwp_call_async_gen_intro(struct llarp_async_iwp * iwp, struct iwp_async_intro * intro); 

  
struct iwp_async_introack;
  
typedef void (*iwp_introack_gen_hook)(struct iwp_async_introack *);
  
struct iwp_async_introack
{
  void * user;
  uint8_t * buf;
  size_t sz;
  /** nonce paramter */
  uint8_t * nonce;
  /** remote public key */
  uint8_t * remote_pubkey;
  /** local private key */
  uint8_t * secretkey;
  /** callback */
  iwp_introack_gen_hook hook;
};


void iwp_call_async_gen_introack(struct llarp_async_iwp * iwp, struct iwp_async_introack * introack);
  
void iwp_call_async_verify_introack(struct llarp_async_iwp * iwp, struct iwp_async_introack * introack);

struct iwp_async_token
{
  void * user;
  uint8_t * buf;
  size_t sz;
  uint8_t * nonce;
  uint8_t * sharedkey;
};
  
struct llarp_async_cipher;
struct llarp_cipher_result;

typedef void (*llarp_cipher_complete_hook)(struct llarp_cipher_result *);

struct llarp_cipher_result {
  llarp_buffer_t buff;
  void *user;
  llarp_cipher_complete_hook hook;
};

struct llarp_async_cipher *llarp_async_cipher_new(
  struct llarp_alloc * mem,
    llarp_sharedkey_t key, struct llarp_crypto *crypto,
    struct llarp_threadpool *result, struct llarp_threadpool *worker);

void llarp_async_cipher_free(struct llarp_async_cipher **c);

void llarp_async_cipher_queue_op(struct llarp_async_cipher *c,
                                 llarp_buffer_t *buff, llarp_nounce_t n,
                                 llarp_cipher_complete_hook h, void *user);

#ifdef __cplusplus
}
#endif
#endif
