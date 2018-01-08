#include <sarp/crypto.h>
#include <sodium/crypto_stream_xchacha20.h>
#include <sodium/crypto_generichash.h>
#include <sodium/crypto_scalarmult.h>

namespace sarp
{
  namespace sodium
  {
    int xchacha20(sarp_buffer_t buff, sarp_symkey_t k, sarp_nounce_t n)
    {
      return crypto_stream_xchacha20_xor(buff.base, buff.base, buff.sz, n, k);
    }

    int dh(sarp_sharedkey_t * shared, uint8_t * client_pk, uint8_t * server_pk, uint8_t * remote_key, uint8_t * local_key)
    {
      uint8_t * out = *shared;
      const size_t outsz = sizeof(sarp_sharedkey_t);
      crypto_generichash_state h;
      if(crypto_scalarmult(out, local_key, remote_key) == -1) return -1;
      crypto_generichash_init(&h, NULL, 0U, outsz);
      crypto_generichash_update(&h, client_pk, sizeof(sarp_pubkey_t));
      crypto_generichash_update(&h, server_pk, sizeof(sarp_pubkey_t));
      crypto_generichash_update(&h, out, crypto_scalarmult_BYTES);
      crypto_generichash_final(&h, out, outsz);
      return 0;
    }

    int dh_client(sarp_sharedkey_t * shared, sarp_pubkey_t pk, sarp_seckey_t sk)
    {
      sarp_pubkey_t local_pk;
      crypto_scalarmult_base(local_pk, sk);
      return dh(shared, local_pk, pk, pk, sk);
    }

    int dh_server(sarp_sharedkey_t * shared, sarp_pubkey_t pk, sarp_seckey_t sk)
    {
      sarp_pubkey_t local_pk;
      crypto_scalarmult_base(local_pk, sk);
      return dh(shared, pk, local_pk, pk, sk);
    }
  }
}

extern "C" {
  void sarp_crypto_libsodium_init(struct sarp_crypto * c)
  {
    c->xchacha20 = sarp::sodium::xchacha20;
    c->dh_client = sarp::sodium::dh_client;
    c->dh_server = sarp::sodium::dh_server;
  }
}
