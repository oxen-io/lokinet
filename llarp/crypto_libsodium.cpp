#include <assert.h>
#include <llarp/crypto.h>
#include <sodium.h>

namespace llarp {
namespace sodium {
static bool xchacha20(llarp_buffer_t buff, llarp_sharedkey_t k,
                      llarp_nounce_t n) {
  uint8_t *base = (uint8_t *)buff.base;
  return crypto_stream_xchacha20_xor(base, base, buff.sz, n, k) == 0;
}

static bool dh(uint8_t * out, uint8_t *client_pk,
               uint8_t *server_pk, uint8_t *remote_key, uint8_t *local_key) {
  crypto_generichash_state h;
  const size_t outsz = SHAREDKEYSIZE;
  if (crypto_scalarmult(out, local_key, remote_key) == -1) return false;
  crypto_generichash_init(&h, NULL, 0U, outsz);
  crypto_generichash_update(&h, client_pk, sizeof(llarp_pubkey_t));
  crypto_generichash_update(&h, server_pk, sizeof(llarp_pubkey_t));
  crypto_generichash_update(&h, out, crypto_scalarmult_BYTES);
  crypto_generichash_final(&h, out, outsz);
  return true;
}

static bool dh_client(llarp_sharedkey_t *shared, llarp_pubkey_t pk,
                      llarp_tunnel_nounce_t n, llarp_seckey_t sk) {
  if (dh(*shared, llarp_seckey_topublic(sk), pk, pk, sk)) {
    return crypto_generichash(*shared, SHAREDKEYSIZE, *shared, SHAREDKEYSIZE, n,
                              TUNNOUNCESIZE) != -1;
  }
  return false;
}

static bool dh_server(llarp_sharedkey_t *shared, llarp_pubkey_t pk,
                      llarp_tunnel_nounce_t n, llarp_seckey_t sk) {
  if (dh(*shared, pk, llarp_seckey_topublic(sk), pk, sk)) {
    return crypto_generichash(*shared, SHAREDKEYSIZE, *shared, SHAREDKEYSIZE, n,
                              TUNNOUNCESIZE) != -1;
  }
  return false;
}


static bool transport_dh_client(uint8_t *shared, uint8_t * pk,
                                uint8_t * sk, uint8_t * n) {
  if (dh(shared, llarp_seckey_topublic(sk), pk, pk, sk)) {
    return crypto_generichash(shared, SHAREDKEYSIZE, shared, SHAREDKEYSIZE, n,
                              NOUNCESIZE) != -1;
  }
  return false;
}

static bool transport_dh_server(uint8_t *shared, uint8_t * pk,
                                uint8_t * sk, uint8_t * n) {
  if (dh(shared, pk, llarp_seckey_topublic(sk), pk, sk)) {
    return crypto_generichash(shared, SHAREDKEYSIZE, shared, SHAREDKEYSIZE, n,
                              NOUNCESIZE) != -1;
  }
  return false;
}

static bool hash(llarp_hash_t *result, llarp_buffer_t buff) {
  const uint8_t *base = (const uint8_t *)buff.base;
  return crypto_generichash(*result, HASHSIZE, base, buff.sz, nullptr, 0) != -1;
}
  
static bool shorthash(llarp_shorthash_t *result, llarp_buffer_t buff) {
  const uint8_t *base = (const uint8_t *)buff.base;
  return crypto_generichash(*result, SHORTHASHSIZE, base, buff.sz, nullptr, 0) != -1;
}

static bool hmac(uint8_t *result, llarp_buffer_t buff,
                 const uint8_t * secret) {
  const uint8_t *base = (const uint8_t *)buff.base;
  return crypto_generichash(result, sizeof(llarp_hash_t), base, buff.sz,
                            secret, HMACSECSIZE) != -1;
}

static bool sign(uint8_t *result, llarp_seckey_t secret,
                 llarp_buffer_t buff) {
  const uint8_t *base = (const uint8_t *)buff.base;
  return crypto_sign_detached(result, nullptr, base, buff.sz, secret) != -1;
}

static bool verify(const uint8_t * pub, llarp_buffer_t buff, const uint8_t * sig) {
  const uint8_t *base = (const uint8_t *)buff.base;
  return crypto_sign_verify_detached(sig, base, buff.sz, pub) != -1;
}

static void randomize(llarp_buffer_t buff) {
  randombytes((unsigned char *)buff.base, buff.sz);
}

static inline void randbytes(void *ptr, size_t sz) {
  randombytes((unsigned char *)ptr, sz);
}

static void keygen(uint8_t *keys) {
  unsigned char seed[32];
  uint8_t *pk = llarp_seckey_topublic(keys);
  crypto_sign_seed_keypair(pk, keys, seed);
}
}  // namespace sodium
}  // namespace llarp

extern "C" {
void llarp_crypto_libsodium_init(struct llarp_crypto *c) {
  assert(sodium_init() != -1);
  c->xchacha20 = llarp::sodium::xchacha20;
  c->dh_client = llarp::sodium::dh_client;
  c->dh_server = llarp::sodium::dh_server;
  c->transport_dh_client = llarp::sodium::transport_dh_client;
  c->transport_dh_server = llarp::sodium::transport_dh_server;
  c->hash = llarp::sodium::hash;
  c->shorthash = llarp::sodium::shorthash;
  c->hmac = llarp::sodium::hmac;
  c->sign = llarp::sodium::sign;
  c->verify = llarp::sodium::verify;
  c->randomize = llarp::sodium::randomize;
  c->randbytes = llarp::sodium::randbytes;
  c->keygen = llarp::sodium::keygen;
}
}
