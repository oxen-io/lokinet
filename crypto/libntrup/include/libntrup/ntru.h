#ifndef LIBNTRUP_NTRU_H
#define LIBNTRUP_NTRU_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "ntru_api.h"

  void
  ntru_init(int force_no_avx2);

  int
  crypto_kem_enc(unsigned char *cstr, unsigned char *k,
                 const unsigned char *pk);

  int
  crypto_kem_dec(unsigned char *k, const unsigned char *cstr,
                 const unsigned char *sk);

  int
  crypto_kem_keypair(unsigned char *pk, unsigned char *sk);

#define crypto_kem_SECRETKEYBYTES 1600
#define crypto_kem_PUBLICKEYBYTES 1218
#define crypto_kem_CIPHERTEXTBYTES 1047

#define NTRU_SECRETKEYBYTES CRYPTO_SECRETKEYBYTES
#define NTRU_PUBLICKEYBYTES CRYPTO_PUBLICKEYBYTES
#define NTRU_CIPHERTEXTBYTES CRYPTO_CIPHERTEXTBYTES

#define CRYPTO_BYTES 32

#ifdef __cplusplus
}
#endif
#endif
