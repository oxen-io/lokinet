#ifndef LLARP_CRYPTO_H_
#define LLARP_CRYPTO_H_
#include <llarp/buffer.h>
#include <llarp/common.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define PUBKEYSIZE 32
#define SECKEYSIZE 64
#define NONCESIZE 24
#define SHAREDKEYSIZE 32
#define HASHSIZE 64
#define SHORTHASHSIZE 32
#define HMACSECSIZE 32
#define SIGSIZE 64
#define TUNNONCESIZE 32
#define HMACSIZE 32

typedef byte_t llarp_pubkey_t[PUBKEYSIZE];
typedef byte_t llarp_seckey_t[SECKEYSIZE];
typedef byte_t llarp_nonce_t[NONCESIZE];
typedef byte_t llarp_sharedkey_t[SHAREDKEYSIZE];
typedef byte_t llarp_hash_t[HASHSIZE];
typedef byte_t llarp_shorthash_t[SHORTHASHSIZE];
typedef byte_t llarp_hmac_t[HMACSIZE];
typedef byte_t llarp_hmacsec_t[HMACSECSIZE];
typedef byte_t llarp_sig_t[SIGSIZE];
typedef byte_t llarp_tunnel_nonce_t[TUNNONCESIZE];

byte_t *
llarp_seckey_topublic(byte_t *secret);

typedef bool (*llarp_dh_func)(llarp_sharedkey_t *, llarp_pubkey_t,
                              llarp_tunnel_nonce_t, llarp_seckey_t);

typedef bool (*llarp_transport_dh_func)(byte_t *, byte_t *, byte_t *, byte_t *);

typedef bool (*llarp_sym_cipher_func)(llarp_buffer_t, llarp_sharedkey_t,
                                      llarp_nonce_t);

typedef bool (*llarp_hash_func)(byte_t *, llarp_buffer_t);

typedef bool (*llarp_shorthash_func)(byte_t *, llarp_buffer_t);

typedef bool (*llarp_hmac_func)(byte_t *, llarp_buffer_t, const byte_t *);

typedef bool (*llarp_sign_func)(byte_t *, const byte_t *, llarp_buffer_t);

typedef bool (*llarp_verify_func)(const byte_t *, llarp_buffer_t,
                                  const byte_t *);

struct llarp_crypto
{
  llarp_sym_cipher_func xchacha20;
  llarp_dh_func dh_client;
  llarp_dh_func dh_server;
  llarp_transport_dh_func transport_dh_client;
  llarp_transport_dh_func transport_dh_server;
  llarp_hash_func hash;
  llarp_shorthash_func shorthash;
  llarp_hmac_func hmac;
  llarp_sign_func sign;
  llarp_verify_func verify;
  void (*randomize)(llarp_buffer_t);
  void (*randbytes)(void *, size_t);
  void (*identity_keygen)(byte_t *);
  void (*encryption_keygen)(byte_t *);
};

void
llarp_crypto_libsodium_init(struct llarp_crypto *c);

bool
llarp_crypto_initialized(struct llarp_crypto *c);

#ifdef __cplusplus
}
#endif

#endif
