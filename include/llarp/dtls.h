#ifndef LLARP_DTLS_H_
#define LLARP_DTLS_H_

#include <llarp/link.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_dtls_args {
  char key_file[255];
  char cert_file[255];
};

bool dtls_link_init(struct llarp_link* link, struct llarp_dtls_args args,
                    struct llarp_msg_muxer* muxer);


#endif
