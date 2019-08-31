#include <libntrup/ntru.h>
#include <stdbool.h>

#include <stdio.h>  // printf

#if __AVX2__
#include <cpuid.h>
#include <array>

std::array< int, 4 >
CPUID(int funcno)
{
  std::array< int, 4 > cpuinfo;
  __cpuid(funcno, cpuinfo[0], cpuinfo[1], cpuinfo[2], cpuinfo[3]);
  return cpuinfo;
}

bool
supports_avx2()
{
  return CPUID(0).at(0) >= 7 && CPUID(7).at(1) & (1 << 5);
}

#else

bool
supports_avx2()
{
  return false;
}

#endif

int (*__crypto_kem_enc)(unsigned char *cstr, unsigned char *k,
                        const unsigned char *pk);

int (*__crypto_kem_dec)(unsigned char *k, const unsigned char *cstr,
                        const unsigned char *sk);

int (*__crypto_kem_keypair)(unsigned char *pk, unsigned char *sk);

extern "C"
{
  void
  ntru_init(int force_no_avx2)
  {
    if(supports_avx2() && !force_no_avx2)
    {
      __crypto_kem_dec     = &crypto_kem_dec_avx2;
      __crypto_kem_enc     = &crypto_kem_enc_avx2;
      __crypto_kem_dec     = &crypto_kem_dec_avx2;
      __crypto_kem_keypair = &crypto_kem_keypair_avx2;
    }
    else
    {
      __crypto_kem_dec     = &crypto_kem_dec_ref;
      __crypto_kem_enc     = &crypto_kem_enc_ref;
      __crypto_kem_dec     = &crypto_kem_dec_ref;
      __crypto_kem_keypair = &crypto_kem_keypair_ref;
    }
  }

  int
  crypto_kem_enc(unsigned char *cstr, unsigned char *k, const unsigned char *pk)
  {
    return __crypto_kem_enc(cstr, k, pk);
  }

  int
  crypto_kem_dec(unsigned char *k, const unsigned char *cstr,
                 const unsigned char *sk)
  {
    return __crypto_kem_dec(k, cstr, sk);
  }

  int
  crypto_kem_keypair(unsigned char *pk, unsigned char *sk)
  {
    return __crypto_kem_keypair(pk, sk);
  }
}
