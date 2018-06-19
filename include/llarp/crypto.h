#ifndef LLARP_CRYPTO_H_
#define LLARP_CRYPTO_H_
#include <llarp/buffer.h>
#include <llarp/common.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * crypto.h
 *
 * libsodium abstraction layer
 * potentially allow libssl support in the future
 */

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
#define PATHIDSIZE 16

/*
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
*/

/// label functors

/// PKE(result, publickey, nonce, secretkey)
typedef bool (*llarp_path_dh_func)(byte_t *, byte_t *, byte_t *, byte_t *);

/// TKE(result publickey, secretkey, nonce)
typedef bool (*llarp_transport_dh_func)(byte_t *, byte_t *, byte_t *, byte_t *);

/// SD/SE(buffer, key, nonce)
typedef bool (*llarp_sym_cipher_func)(llarp_buffer_t, const byte_t *,
                                      const byte_t *);

/// H(result, body)
typedef bool (*llarp_hash_func)(byte_t *, llarp_buffer_t);

/// SH(result, body)
typedef bool (*llarp_shorthash_func)(byte_t *, llarp_buffer_t);

/// MDS(result, body, shared_secret)
typedef bool (*llarp_hmac_func)(byte_t *, llarp_buffer_t, const byte_t *);

/// S(sig, secretkey, body)
typedef bool (*llarp_sign_func)(byte_t *, const byte_t *, llarp_buffer_t);

/// V(sig, body, secretkey)
typedef bool (*llarp_verify_func)(const byte_t *, llarp_buffer_t,
                                  const byte_t *);

/// library crypto configuration
struct llarp_crypto
{
  /// xchacha symettric cipher
  llarp_sym_cipher_func xchacha20;
  /// path dh creator's side
  llarp_path_dh_func dh_client;
  /// path dh relay side
  llarp_path_dh_func dh_server;
  /// transport dh client side
  llarp_transport_dh_func transport_dh_client;
  /// transport dh server side
  llarp_transport_dh_func transport_dh_server;
  /// blake2b 512 bit
  llarp_hash_func hash;
  /// blake2b 256 bit
  llarp_shorthash_func shorthash;
  /// blake2s 256 bit hmac
  llarp_hmac_func hmac;
  /// ed25519 sign
  llarp_sign_func sign;
  /// ed25519 verify
  llarp_verify_func verify;
  /// randomize buffer
  void (*randomize)(llarp_buffer_t);
  /// randomizer memory
  void (*randbytes)(void *, size_t);
  /// generate signing keypair
  void (*identity_keygen)(byte_t *);
  /// generate encryption keypair
  void (*encryption_keygen)(byte_t *);
};

/// set crypto function pointers to use libsodium
void
llarp_crypto_libsodium_init(struct llarp_crypto *c);

/// check for initialize crypto
bool
llarp_crypto_initialized(struct llarp_crypto *c);

#ifdef __cplusplus
}
#endif

#endif
