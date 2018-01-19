#ifndef SARP_CRYPTO_H_
#define SARP_CRYPTO_H_
#include <sarp/buffer.h>
#include <stdint.h>
#include <stdbool.h>
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

  typedef uint8_t sarp_pubkey_t[PUBKEYSIZE];
  typedef uint8_t sarp_seckey_t[SECKEYSIZE];
  typedef uint8_t sarp_nounce_t[NOUNCESIZE];
  typedef uint8_t sarp_sharedkey_t[SHAREDKEYSIZE];
  typedef uint8_t sarp_hash_t[HASHSIZE];
  typedef uint8_t sarp_hmacsec_t[HMACSECSIZE];
  typedef uint8_t sarp_sig_t[SIGSIZE];
  typedef uint8_t sarp_tunnel_nounce_t[TUNNOUNCESIZE];

  struct sarp_crypto
  {
    bool (*xchacha20)(sarp_buffer_t, sarp_sharedkey_t, sarp_nounce_t);
    bool (*dh_client)(sarp_sharedkey_t *, sarp_pubkey_t, sarp_tunnel_nounce_t, sarp_seckey_t);
    bool (*dh_server)(sarp_sharedkey_t *, sarp_pubkey_t, sarp_tunnel_nounce_t, sarp_seckey_t);
    bool (*hash)(sarp_hash_t *, sarp_buffer_t);
    bool (*hmac)(sarp_hash_t *, sarp_buffer_t, sarp_hmacsec_t);
    bool (*sign)(sarp_sig_t *, sarp_seckey_t, sarp_buffer_t);
    bool (*verify)(sarp_pubkey_t, sarp_buffer_t, sarp_sig_t);
  };

  void sarp_crypto_libsodium_init(struct sarp_crypto * c);

#ifdef __cplusplus
}
#endif
  
#endif
