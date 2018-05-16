#ifndef LLARP_QUIC_H_
#define LLARP_QUIC_H_

#include <llarp/link.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_quic_args {
};

bool quic_link_init(struct llarp_link* link, struct llarp_quic_args args,
                    struct llarp_msg_muxer* muxer);

  
#ifdef __cplusplus
}
#endif
#endif
