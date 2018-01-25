#include <llarp/crypto.h>
#include <sodium/crypto_stream_xchacha20.h>
#include <sodium/crypto_generichash.h>
#include <sodium/crypto_scalarmult.h>
#include <sodium/crypto_sign.h>

namespace llarp
{
  namespace sodium
  {
    bool xchacha20(llarp_buffer_t buff, llarp_sharedkey_t k, llarp_nounce_t n)
    {
      uint8_t * base = (uint8_t*)buff.base;
      return crypto_stream_xchacha20_xor(base, base, buff.sz, n, k) == 0;
    }

    bool dh(llarp_sharedkey_t * shared, uint8_t * client_pk, uint8_t * server_pk, uint8_t * remote_key, uint8_t * local_key)
    {
      uint8_t * out = *shared;
      const size_t outsz = SHAREDKEYSIZE;
      crypto_generichash_state h;
      if(crypto_scalarmult(out, local_key, remote_key) == -1) return false;
      crypto_generichash_init(&h, NULL, 0U, outsz);
      crypto_generichash_update(&h, client_pk, sizeof(llarp_pubkey_t));
      crypto_generichash_update(&h, server_pk, sizeof(llarp_pubkey_t));
      crypto_generichash_update(&h, out, crypto_scalarmult_BYTES);
      crypto_generichash_final(&h, out, outsz);
      return true;
    }

    bool dh_client(llarp_sharedkey_t * shared, llarp_pubkey_t pk, llarp_tunnel_nounce_t n, llarp_seckey_t sk)
    {
      llarp_pubkey_t local_pk;
      crypto_scalarmult_base(local_pk, sk);
      if(dh(shared, local_pk, pk, pk, sk))
      {
        return crypto_generichash(*shared, SHAREDKEYSIZE, *shared, SHAREDKEYSIZE, n, TUNNOUNCESIZE) != -1;
      }
      return false;
    }

    bool dh_server(llarp_sharedkey_t * shared, llarp_pubkey_t pk, llarp_tunnel_nounce_t n, llarp_seckey_t sk)
    {
      llarp_pubkey_t local_pk;
      crypto_scalarmult_base(local_pk, sk);
      if(dh(shared, pk, local_pk, pk, sk))
      {
        return crypto_generichash(*shared, SHAREDKEYSIZE, *shared, SHAREDKEYSIZE, n, TUNNOUNCESIZE) != -1;
      }
      return false;
    }

    bool hash(llarp_hash_t * result, llarp_buffer_t buff)
    {
      const uint8_t * base = (const uint8_t *) buff.base;
      return crypto_generichash(*result, HASHSIZE, base, buff.sz, nullptr, 0) != -1;
    }

    bool hmac(llarp_hash_t * result, llarp_buffer_t buff, llarp_seckey_t secret)
    {
      const uint8_t * base = (const uint8_t *) buff.base;
      return crypto_generichash(*result, sizeof(llarp_hash_t), base, buff.sz, secret, HMACSECSIZE) != -1;
    }

    bool sign(llarp_sig_t * result, llarp_seckey_t secret, llarp_buffer_t buff)
    {
      const uint8_t * base = (const uint8_t *) buff.base;
      return crypto_sign_detached(*result, nullptr, base, buff.sz, secret) != -1;
    }

    bool verify(llarp_pubkey_t pub, llarp_buffer_t buff, llarp_sig_t sig)
    {
      const uint8_t * base = (const uint8_t *) buff.base;
      return crypto_sign_verify_detached(sig, base, buff.sz, pub) != -1;
    }
  }
}

extern "C" {
  void llarp_crypto_libsodium_init(struct llarp_crypto * c)
  {
    c->xchacha20 = llarp::sodium::xchacha20;
    c->dh_client = llarp::sodium::dh_client;
    c->dh_server = llarp::sodium::dh_server;
    c->hash = llarp::sodium::hash;
    c->hmac = llarp::sodium::hmac;
    c->sign = llarp::sodium::sign;
    c->verify = llarp::sodium::verify;
  }
}
