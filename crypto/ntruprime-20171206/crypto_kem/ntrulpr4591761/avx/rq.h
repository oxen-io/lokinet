#ifndef rq_h
#define rq_h

#include "modq.h"
#include "small.h"

#define rq_encode crypto_kem_ntrulpr4591761_avx_rq_encode
extern void rq_encode(unsigned char *,const modq *);

#define rq_decode crypto_kem_ntrulpr4591761_avx_rq_decode
extern void rq_decode(modq *,const unsigned char *);

#define rq_roundencode crypto_kem_ntrulpr4591761_avx_rq_roundencode
extern void rq_roundencode(unsigned char *,const modq *);

#define rq_decoderounded crypto_kem_ntrulpr4591761_avx_rq_decoderounded
extern void rq_decoderounded(modq *,const unsigned char *);

#define rq_round3 crypto_kem_ntrulpr4591761_avx_rq_round
extern void rq_round3(modq *,const modq *);

#define rq_mult crypto_kem_ntrulpr4591761_avx_rq_mult
extern void rq_mult(modq *,const modq *,const small *);

#define rq_recip3 crypto_kem_ntrulpr4591761_avx_rq_recip3
int rq_recip3(modq *,const small *);

#define rq_fromseed crypto_kem_ntrulpr4591761_avx_rq_fromseed
extern void rq_fromseed(modq *,const unsigned char *);

#define rq_top crypto_kem_ntrulpr4591761_avx_rq_top
extern void rq_top(unsigned char *,const modq *,const unsigned char *);

#define rq_rightsubbit crypto_kem_ntrulpr4591761_avx_rq_rightsubbit
extern void rq_rightsubbit(unsigned char *,const unsigned char *,const modq *);

#endif
