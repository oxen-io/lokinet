#include <libntrup/ntru.h>
#include <stdbool.h>


#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>

static bool supports_avx2()
{
  int cpuinfo[4] = {-1};
  __cpuid(0, cpuinfo[0], cpuinfo[1], cpuinfo[2], cpuinfo[3]);
  if(cpuinfo[0] < 7)
    return false;
    __cpuid(7, cpuinfo[0], cpuinfo[1], cpuinfo[2], cpuinfo[3]);
  return cpuinfo[1] & (1 << 5);
}


#else

static bool supports_avx2()
{
  return false;
}

#endif


int (*__crypto_kem_enc)(unsigned char *cstr, unsigned char *k, const unsigned char *pk);
  
int (*__crypto_kem_dec)(unsigned char *k, const unsigned char *cstr, const unsigned char *sk);

int (*__crypto_kem_keypair)(unsigned char *pk, unsigned char * sk);

void ntru_init()
{
  if(supports_avx2())
  {
    __crypto_kem_dec = crypto_kem_dec_avx2;
    __crypto_kem_enc = crypto_kem_enc_avx2;
    __crypto_kem_dec = crypto_kem_dec_avx2;
  }
  else
  {
    __crypto_kem_dec = crypto_kem_dec_ref;
    __crypto_kem_enc = crypto_kem_enc_ref;
    __crypto_kem_dec = crypto_kem_dec_ref;
  }
}


int crypto_kem_enc(unsigned char *cstr, unsigned char *k, const unsigned char *pk)
{
  return __crypto_kem_enc(cstr, k, pk);
}
  
int crypto_kem_dec(unsigned char *k, const unsigned char *cstr, const unsigned char *sk) 
{
  return __crypto_kem_dec(k, cstr, sk);
}

int crypto_kem_keypair(unsigned char *pk, unsigned char * sk) 
{
 return __crypto_kem_keypair(pk, sk);
}