#include <libntrup/ntru.h>

extern "C"
{
  int
  crypto_kem_enc(unsigned char* cstr, unsigned char* k, const unsigned char* pk)
  {
    return crypto_kem_enc_ref(cstr, k, pk);
  }

  int
  crypto_kem_dec(unsigned char* k, const unsigned char* cstr, const unsigned char* sk)
  {
    return crypto_kem_dec_ref(k, cstr, sk);
  }

  int
  crypto_kem_keypair(unsigned char* pk, unsigned char* sk)
  {
    return crypto_kem_keypair_ref(pk, sk);
  }
}
