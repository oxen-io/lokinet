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

#include <libntrup/ntru.h>

#define PQ_CIPHERTEXTSIZE crypto_kem_CIPHERTEXTBYTES
#define PQ_PUBKEYSIZE crypto_kem_PUBLICKEYBYTES
#define PQ_SECRETKEYSIZE crypto_kem_SECRETKEYBYTES
#define PQ_KEYPAIRSIZE (PQ_SECRETKEYSIZE + PQ_SECRETKEYSIZE)

/// label functors

/// PKE(result, publickey, secretkey, nonce)
typedef bool (*llarp_path_dh_func)(byte_t *, const byte_t *, const byte_t *,
                                   const byte_t *);

/// TKE(result, publickey, secretkey, nonce)
typedef bool (*llarp_transport_dh_func)(byte_t *, const byte_t *,
                                        const byte_t *, const byte_t *);

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

/// V(pubkey, body, sig)
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
  /// generate post quantum encrytion key
  void (*pqe_keygen)(byte_t *);
  /// post quantum decrypt (buffer, sharedkey_dst, sec)
  bool (*pqe_decrypt)(const byte_t *, byte_t *, const byte_t *);
  /// post quantum encrypt (buffer, sharedkey_dst,  pub)
  bool (*pqe_encrypt)(byte_t *, byte_t *, const byte_t *);
};

/// initialize crypto subsystem
void
llarp_crypto_init(struct llarp_crypto *c);

/// check for initialize crypto
bool
llarp_crypto_initialized(struct llarp_crypto *c);

/// return random 64bit unsigned interger
uint64_t
llarp_randint();

#endif
