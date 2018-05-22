#ifndef LLARP_DTLS_H_
#define LLARP_DTLS_H_

#include <llarp/link.h>
#include <llarp/mem.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_dtls_args
{
  struct llarp_alloc* mem;
  const char* keyfile;
  const char* certfile;
};

void
dtls_link_init(struct llarp_link* link, struct llarp_dtls_args args,
               struct llarp_msg_muxer* muxer);

#ifdef __cplusplus
}
#endif
#endif
