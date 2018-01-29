#ifndef LLARP_CRYPTO_H_
#define LLARP_CRYPTO_H_
#include <llarp/buffer.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define PUBKEYSIZE 32
#define SECKEYSIZE 32
#define NOUNCESIZE 24
#define SHAREDKEYSIZE 32
#define HASHSIZE 64
#define HMACSECSIZE 32
#define SIGSIZE 64
#define TUNNOUNCESIZE 32

typedef uint8_t llarp_pubkey_t[PUBKEYSIZE];
typedef uint8_t llarp_seckey_t[SECKEYSIZE];
typedef uint8_t llarp_nounce_t[NOUNCESIZE];
typedef uint8_t llarp_sharedkey_t[SHAREDKEYSIZE];
typedef uint8_t llarp_hash_t[HASHSIZE];
typedef uint8_t llarp_hmacsec_t[HMACSECSIZE];
typedef uint8_t llarp_sig_t[SIGSIZE];
typedef uint8_t llarp_tunnel_nounce_t[TUNNOUNCESIZE];

struct llarp_crypto {
  bool (*xchacha20)(llarp_buffer_t, llarp_sharedkey_t, llarp_nounce_t);
  bool (*dh_client)(llarp_sharedkey_t *, llarp_pubkey_t, llarp_tunnel_nounce_t,
                    llarp_seckey_t);
  bool (*dh_server)(llarp_sharedkey_t *, llarp_pubkey_t, llarp_tunnel_nounce_t,
                    llarp_seckey_t);
  bool (*hash)(llarp_hash_t *, llarp_buffer_t);
  bool (*hmac)(llarp_hash_t *, llarp_buffer_t, llarp_hmacsec_t);
  bool (*sign)(llarp_sig_t *, llarp_seckey_t, llarp_buffer_t);
  bool (*verify)(llarp_pubkey_t, llarp_buffer_t, llarp_sig_t);
};

void llarp_crypto_libsodium_init(struct llarp_crypto *c);

#ifdef __cplusplus
}
#endif

#endif
