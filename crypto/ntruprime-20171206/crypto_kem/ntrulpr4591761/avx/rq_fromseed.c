#include "crypto_stream_aes256ctr.h"
#include "rq.h"
#include "params.h"

static const unsigned char n[16] = {0};

void rq_fromseed(modq *h,const unsigned char *K)
{
  crypto_uint32 buf[768];
  int i;

  crypto_stream_aes256ctr((unsigned char *) buf,sizeof buf,n,K);
  /* will use 761*4 bytes */
  /* convenient for aes to generate multiples of 16 bytes */
  /* and multiples of more for some implementations */

  for (i = 0;i < p;++i)
    h[i] = modq_fromuint32(buf[i]);
  for (i = p;i < 768;++i)
    h[i] = 0;
}
