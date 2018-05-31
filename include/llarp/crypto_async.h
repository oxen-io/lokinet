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

/// context for doing asynchronous cryptography for iwp
/// with a worker threadpool
/// defined in crypto_async.cpp
struct llarp_async_iwp;

/// allocator
/// use crypto as cryptograph implementation
/// use logic as the callback handler thread
/// use worker as threadpool that does the heavy lifting
struct llarp_async_iwp *
llarp_async_iwp_new(struct llarp_crypto *crypto, struct llarp_logic *logic,
                    struct llarp_threadpool *worker);

/// deallocator
void
llarp_async_iwp_free(struct llarp_async_iwp *iwp);


/// context for doing asynchronous cryptography for rc
/// with a worker threadpool
/// defined in crypto_async.cpp
struct llarp_async_rc;

/// rc async context allocator
struct llarp_async_rc *
llarp_async_rc_new(struct llarp_crypto *crypto, struct llarp_logic *logic,
                  struct llarp_threadpool *worker);

/// deallocator
void
llarp_async_rc_free(struct llarp_async_rc *rc);


struct iwp_async_keygen;

/// define functor for keygen
typedef void (*iwp_keygen_hook)(struct iwp_async_keygen *);

/// key generation request
struct iwp_async_keygen
{
  /// internal wire protocol async configuration
  struct llarp_async_iwp *iwp;
  /// a pointer to pass ourself to thread worker
  void *user;
  /// destination key buffer
  uint8_t *keybuf;
  /// result handler callback
  iwp_keygen_hook hook;
};

/// generate an encryption keypair asynchronously
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

/// asynchronously generate an intro packet
void
iwp_call_async_gen_intro(struct llarp_async_iwp *iwp,
                         struct iwp_async_intro *intro);

/// asynchronously verify an intro packet
void
iwp_call_async_verify_intro(struct llarp_async_iwp *iwp,
                            struct iwp_async_intro *info);

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

/// generate introduction acknowledgement packet asynchronously
void
iwp_call_async_gen_introack(struct llarp_async_iwp *iwp,
                            struct iwp_async_introack *introack);

/// verify introduction acknowledgement packet asynchronously
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
  /// nonce parameter
  uint8_t *nonce;
  /// token parameter
  uint8_t *token;
  /// memory to write session key to
  uint8_t *sessionkey;
  /// local secrkey key
  uint8_t *secretkey;
  /// remote public encryption key
  uint8_t *remote_pubkey;
  /// result callback handler
  iwp_session_start_hook hook;
};

/// generate session start packet asynchronously
void
iwp_call_async_gen_session_start(struct llarp_async_iwp *iwp,
                                 struct iwp_async_session_start *start);

/// verify session start packet asynchronously
void
iwp_call_async_verify_session_start(struct llarp_async_iwp *iwp,
                                    struct iwp_async_session_start *start);

struct iwp_async_frame;

/// internal wire protocol frame request
typedef void (*iwp_async_frame_hook)(struct iwp_async_frame *);

struct iwp_async_frame
{
  /// true if decryption succeded
  bool success;
  struct llarp_async_iwp *iwp;
  /// a pointer to pass ourself
  void *user;
  /// current session key
  uint8_t *sessionkey;
  /// size of the frame
  size_t sz;
  /// result handler
  iwp_async_frame_hook hook;
  /// memory holding the entire frame
  uint8_t buf[1500];
};

/// decrypt iwp frame asynchronously
void
iwp_call_async_frame_decrypt(struct llarp_async_iwp *iwp,
                             struct iwp_async_frame *frame);

/// encrypt iwp frame asynchronously
void
iwp_call_async_frame_encrypt(struct llarp_async_iwp *iwp,
                             struct iwp_async_frame *frame);



/// define functor for rc_verify
typedef void (*rc_verify_hook)(struct rc_async_verify *);

/// rc verify request
struct rc_async_verify
{
  /// router contact crypto async configuration
  struct llarp_async_rc *context;
  /// a pointer to pass ourself to thread worker
  void *self;
  /// the router contact
  struct llarp_rc *rc;
  /// result
  bool result;
  /// result handler callback
  rc_verify_hook hook;
  /// extra data to pass to hook
  void *hook_user;
};


/// rc verify
void rc_call_async_verify(struct llarp_async_rc *context,
                          struct rc_async_verify *request,
                          struct llarp_rc *rc);

#ifdef __cplusplus
}
#endif
#endif
