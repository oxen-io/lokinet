#ifndef LLARP_DTLS_H_
#define LLARP_DTLS_H_

#include <llarp/mem.h>
#include <llarp/link.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_dtls_args {
  struct llarp_alloc * mem;
  char key_file[255];
  char cert_file[255];
};

void dtls_link_init(struct llarp_link* link, struct llarp_dtls_args args,
                    struct llarp_msg_muxer* muxer);


#endif
