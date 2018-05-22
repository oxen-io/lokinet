#ifndef LLARP_CRYPTO_ASYNC_H_
#define LLARP_CRYPTO_ASYNC_H_
#include <llarp/crypto.h>
#include <llarp/ev.h>
#include <llarp/logic.h>
#include <llarp/threadpool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_async_iwp;

struct llarp_async_iwp *
llarp_async_iwp_new(struct llarp_alloc *mem, struct llarp_crypto *crypto,
                    struct llarp_logic *logic, struct llarp_threadpool *worker);

void
llarp_async_iwp_free(struct llarp_async_iwp *iwp);

struct iwp_async_keygen;

typedef void (*iwp_keygen_hook)(struct iwp_async_keygen *);

struct iwp_async_keygen
{
  struct llarp_async_iwp *iwp;
  void *user;
  uint8_t *keybuf;
  iwp_keygen_hook hook;
};

void
iwp_call_async_keygen(struct llarp_async_iwp *iwp,
                      struct iwp_async_keygen *keygen);

struct iwp_async_intro;

typedef void (*iwp_intro_hook)(struct iwp_async_intro *);

struct iwp_async_intro
{
  struct llarp_async_iwp *iwp;
  void *user;
  uint8_t *buf;
  size_t sz;
  /** nonce paramter */
  uint8_t *nonce;
  /** remote public key */
  uint8_t *remote_pubkey;
  /** local private key */
  uint8_t *secretkey;
  /** callback */
  iwp_intro_hook hook;
};

void
iwp_call_async_gen_intro(struct llarp_async_iwp *iwp,
                         struct iwp_async_intro *intro);

struct iwp_async_introack;

typedef void (*iwp_introack_hook)(struct iwp_async_introack *);

struct iwp_async_introack
{
  struct llarp_async_iwp *iwp;
  void *user;
  uint8_t *buf;
  size_t sz;
  /** nonce paramter */
  uint8_t *nonce;
  /** token paramter */
  uint8_t *token;
  /** remote public key */
  uint8_t *remote_pubkey;
  /** local private key */
  uint8_t *secretkey;
  /** callback */
  iwp_introack_hook hook;
};

void
iwp_call_async_gen_introack(struct llarp_async_iwp *iwp,
                            struct iwp_async_introack *introack);

void
iwp_call_async_verify_introack(struct llarp_async_iwp *iwp,
                               struct iwp_async_introack *introack);

struct iwp_async_session_start;

typedef void (*iwp_session_start_hook)(struct iwp_async_session_start *);

struct iwp_async_session_start
{
  struct llarp_async_iwp *iwp;
  void *user;
  uint8_t *buf;
  size_t sz;
  uint8_t *nonce;
  uint8_t *token;
  uint8_t *sessionkey;
  uint8_t *secretkey;
  uint8_t *remote_pubkey;
  iwp_session_start_hook hook;
};

void
iwp_call_async_gen_session_start(struct llarp_async_iwp *iwp,
                                 struct iwp_async_session_start *start);

void
iwp_call_async_verify_session_start(struct llarp_async_iwp *iwp,
                                    struct iwp_async_session_start *start);

struct iwp_async_frame;

typedef void (*iwp_async_frame_hook)(struct iwp_async_frame *);

struct iwp_async_frame
{
  void *user;
  bool success;
  uint8_t *sessionkey;
  size_t sz;
  iwp_async_frame_hook hook;
  uint8_t buf[1500];
};

void
iwp_call_async_frame_decrypt(struct llarp_async_iwp *iwp,
                             struct iwp_async_frame *frame);

void
iwp_call_async_frame_encrypt(struct llarp_async_iwp *iwp,
                             struct iwp_async_frame *frame);

#ifdef __cplusplus
}
#endif
#endif
