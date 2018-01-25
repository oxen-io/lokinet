#include <sarp/crypto.h>
#include <sodium/crypto_stream_xchacha20.h>
#include <sodium/crypto_generichash.h>
#include <sodium/crypto_scalarmult.h>
#include <sodium/crypto_sign.h>

namespace sarp
{
  namespace sodium
  {
    bool xchacha20(sarp_buffer_t buff, sarp_sharedkey_t k, sarp_nounce_t n)
    {
      uint8_t * base = (uint8_t*)buff.base;
      return crypto_stream_xchacha20_xor(base, base, buff.sz, n, k) == 0;
    }

    bool dh(sarp_sharedkey_t * shared, uint8_t * client_pk, uint8_t * server_pk, uint8_t * remote_key, uint8_t * local_key)
    {
      uint8_t * out = *shared;
      const size_t outsz = SHAREDKEYSIZE;
      crypto_generichash_state h;
      if(crypto_scalarmult(out, local_key, remote_key) == -1) return false;
      crypto_generichash_init(&h, NULL, 0U, outsz);
      crypto_generichash_update(&h, client_pk, sizeof(sarp_pubkey_t));
      crypto_generichash_update(&h, server_pk, sizeof(sarp_pubkey_t));
      crypto_generichash_update(&h, out, crypto_scalarmult_BYTES);
      crypto_generichash_final(&h, out, outsz);
      return true;
    }

    bool dh_client(sarp_sharedkey_t * shared, sarp_pubkey_t pk, sarp_tunnel_nounce_t n, sarp_seckey_t sk)
    {
      sarp_pubkey_t local_pk;
      crypto_scalarmult_base(local_pk, sk);
      if(dh(shared, local_pk, pk, pk, sk))
      {
        return crypto_generichash(*shared, SHAREDKEYSIZE, *shared, SHAREDKEYSIZE, n, TUNNOUNCESIZE) != -1;
      }
      return false;
    }

    bool dh_server(sarp_sharedkey_t * shared, sarp_pubkey_t pk, sarp_tunnel_nounce_t n, sarp_seckey_t sk)
    {
      sarp_pubkey_t local_pk;
      crypto_scalarmult_base(local_pk, sk);
      if(dh(shared, pk, local_pk, pk, sk))
      {
        return crypto_generichash(*shared, SHAREDKEYSIZE, *shared, SHAREDKEYSIZE, n, TUNNOUNCESIZE) != -1;
      }
      return false;
    }

    bool hash(sarp_hash_t * result, sarp_buffer_t buff)
    {
      const uint8_t * base = (const uint8_t *) buff.base;
      return crypto_generichash(*result, HASHSIZE, base, buff.sz, nullptr, 0) != -1;
    }

    bool hmac(sarp_hash_t * result, sarp_buffer_t buff, sarp_seckey_t secret)
    {
      const uint8_t * base = (const uint8_t *) buff.base;
      return crypto_generichash(*result, sizeof(sarp_hash_t), base, buff.sz, secret, HMACSECSIZE) != -1;
    }

    bool sign(sarp_sig_t * result, sarp_seckey_t secret, sarp_buffer_t buff)
    {
      const uint8_t * base = (const uint8_t *) buff.base;
      return crypto_sign_detached(*result, nullptr, base, buff.sz, secret) != -1;
    }

    bool verify(sarp_pubkey_t pub, sarp_buffer_t buff, sarp_sig_t sig)
    {
      const uint8_t * base = (const uint8_t *) buff.base;
      return crypto_sign_verify_detached(sig, base, buff.sz, pub) != -1;
    }
  }
}

extern "C" {
  void sarp_crypto_libsodium_init(struct sarp_crypto * c)
  {
    c->xchacha20 = sarp::sodium::xchacha20;
    c->dh_client = sarp::sodium::dh_client;
    c->dh_server = sarp::sodium::dh_server;
    c->hash = sarp::sodium::hash;
    c->hmac = sarp::sodium::hmac;
    c->sign = sarp::sodium::sign;
    c->verify = sarp::sodium::verify;
  }
}
