#ifndef LLARP_CRYPTO_ASYNC_H_
#define LLARP_CRYPTO_ASYNC_H_
#include <llarp/crypto.h>
#include <llarp/ev.h>
#include <llarp/logic.h>
#include <llarp/threadpool.h>

/**
 * crypto_async.h
 *
 * asynchronous crypto functions
 */

#ifdef __cplusplus
extern "C" {
#endif

/// defined in crypto_async.cpp
struct llarp_async_iwp;

/// allocator
struct llarp_async_iwp *
llarp_async_iwp_new(struct llarp_alloc *mem, struct llarp_crypto *crypto,
                    struct llarp_logic *logic, struct llarp_threadpool *worker);

/// deallocator
void
llarp_async_iwp_free(struct llarp_async_iwp *iwp);

struct iwp_async_keygen;

/// define functor for keygen
typedef void (*iwp_keygen_hook)(struct iwp_async_keygen *);

/// key generation request
struct iwp_async_keygen
{
  /// internal wire protocol async configuration
  struct llarp_async_iwp *iwp;
  /// a customizable pointer to pass data to iteration functor
  void *user;
  /// destination key buffer
  uint8_t *keybuf;
  /// iteration functor
  iwp_keygen_hook hook;
};

/// generate a key by iterating on "iwp" using "keygen" request
void
iwp_call_async_keygen(struct llarp_async_iwp *iwp,
                      struct iwp_async_keygen *keygen);

struct iwp_async_intro;

/// iwp_async_intro functor
typedef void (*iwp_intro_hook)(struct iwp_async_intro *);

/// iwp_async_intro request
struct iwp_async_intro
{
  struct llarp_async_iwp *iwp;
  void *user;
  uint8_t *buf;
  size_t sz;
  /// nonce paramter
  uint8_t *nonce;
  /// remote public key
  uint8_t *remote_pubkey;
  /// local private key
  uint8_t *secretkey;
  /// callback
  iwp_intro_hook hook;
};

/// introduce internal wire protocol "iwp" using "intro" request
void
iwp_call_async_gen_intro(struct llarp_async_iwp *iwp,
                         struct iwp_async_intro *intro);

struct iwp_async_introack;

/// introduction acknowledgement functor
typedef void (*iwp_introack_hook)(struct iwp_async_introack *);

/// introduction acknowledgement request
struct iwp_async_introack
{
  struct llarp_async_iwp *iwp;
  void *user;
  uint8_t *buf;
  size_t sz;
  /// nonce paramter
  uint8_t *nonce;
  /// token paramter
  uint8_t *token;
  /// remote public key
  uint8_t *remote_pubkey;
  /// local private key
  uint8_t *secretkey;
  /// callback
  iwp_introack_hook hook;
};

/// generate introduction acknowledgement "iwp" using "introack" request
void
iwp_call_async_gen_introack(struct llarp_async_iwp *iwp,
                            struct iwp_async_introack *introack);

/// verify introduction acknowledgement "iwp" using "introack" request
void
iwp_call_async_verify_introack(struct llarp_async_iwp *iwp,
                               struct iwp_async_introack *introack);

struct iwp_async_session_start;

/// start session functor
typedef void (*iwp_session_start_hook)(struct iwp_async_session_start *);

/// start session request
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

/// generate session start "iwp" using "start" request
void
iwp_call_async_gen_session_start(struct llarp_async_iwp *iwp,
                                 struct iwp_async_session_start *start);

/// verify session start "iwp" using "start" request
void
iwp_call_async_verify_session_start(struct llarp_async_iwp *iwp,
                                    struct iwp_async_session_start *start);

struct iwp_async_frame;

/// internal wire protocol frame request
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

/// decrypt iwp frame "iwp" using "frame" request
void
iwp_call_async_frame_decrypt(struct llarp_async_iwp *iwp,
                             struct iwp_async_frame *frame);

/// encrypt iwp frame "iwp" using "frame" request
void
iwp_call_async_frame_encrypt(struct llarp_async_iwp *iwp,
                             struct iwp_async_frame *frame);

#ifdef __cplusplus
}
#endif
#endif
