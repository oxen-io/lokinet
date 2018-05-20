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
#define NOUNCESIZE 24
#define SHAREDKEYSIZE 32
#define HASHSIZE 64
#define SHORTHASHSIZE 32
#define HMACSECSIZE 32
#define SIGSIZE 64
#define TUNNOUNCESIZE 32
#define HMACSIZE 32

typedef uint8_t llarp_pubkey_t[PUBKEYSIZE];
typedef uint8_t llarp_seckey_t[SECKEYSIZE];
typedef uint8_t llarp_nounce_t[NOUNCESIZE];
typedef uint8_t llarp_sharedkey_t[SHAREDKEYSIZE];
typedef uint8_t llarp_hash_t[HASHSIZE];
typedef uint8_t llarp_shorthash_t[SHORTHASHSIZE];
typedef uint8_t llarp_hmac_t[HMACSIZE];
typedef uint8_t llarp_hmacsec_t[HMACSECSIZE];
typedef uint8_t llarp_sig_t[SIGSIZE];
typedef uint8_t llarp_tunnel_nounce_t[TUNNOUNCESIZE];

static INLINE uint8_t *llarp_seckey_topublic(llarp_seckey_t k) {
  return k + 32;
}

typedef bool (*llarp_dh_func)(llarp_sharedkey_t *, llarp_pubkey_t,
                              llarp_tunnel_nounce_t, llarp_seckey_t);

  
typedef bool (*llarp_transport_dh_func)(uint8_t *, uint8_t *,
                                          uint8_t *, uint8_t *);
  
typedef bool (*llarp_sym_cipher_func)(llarp_buffer_t, llarp_sharedkey_t,
                                      llarp_nounce_t);

typedef bool (*llarp_hash_func)(llarp_hash_t *, llarp_buffer_t);

typedef bool (*llarp_shorthash_func)(llarp_shorthash_t *, llarp_buffer_t);

typedef bool (*llarp_hmac_func)(uint8_t *, llarp_buffer_t,
                                const uint8_t *);

typedef bool (*llarp_sign_func)(uint8_t *, llarp_seckey_t, llarp_buffer_t);

typedef bool (*llarp_verify_func)(const uint8_t *, llarp_buffer_t, const uint8_t *);

struct llarp_crypto {
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
  void (*keygen)(uint8_t *);
};

void llarp_crypto_libsodium_init(struct llarp_crypto *c);

bool llarp_crypto_initialized(struct llarp_crypto * c);
  
#ifdef __cplusplus
}
#endif

#endif
