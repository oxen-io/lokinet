
int
crypto_kem_enc_ref(unsigned char *cstr, unsigned char *k,
                   const unsigned char *pk);

int
crypto_kem_dec_ref(unsigned char *k, const unsigned char *cstr,
                   const unsigned char *sk);

int
crypto_kem_keypair_ref(unsigned char *pk, unsigned char *sk);

int
crypto_kem_enc_avx2(unsigned char *cstr, unsigned char *k,
                    const unsigned char *pk);

int
crypto_kem_dec_avx2(unsigned char *k, const unsigned char *cstr,
                    const unsigned char *sk);

int
crypto_kem_keypair_avx2(unsigned char *pk, unsigned char *sk);
