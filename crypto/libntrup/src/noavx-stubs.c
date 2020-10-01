// Stubs for compilers/builds without avx2 support
//
int
crypto_kem_enc_avx2(unsigned char *cstr, unsigned char *k,
                    const unsigned char *pk)
{
  (void)(cstr);
  (void)(k);
  (void)(pk);
  return -1;
}

int
crypto_kem_dec_avx2(unsigned char *k, const unsigned char *cstr,
                    const unsigned char *sk)
{
  (void)(k);
  (void)(sk);
  (void)(cstr);
  return -1;
}

int
crypto_kem_keypair_avx2(unsigned char *pk, unsigned char *sk)
{
  (void)(pk);
  (void)(sk);
  return -1;
}
