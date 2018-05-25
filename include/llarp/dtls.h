#ifndef LLARP_DTLS_H_
#define LLARP_DTLS_H_

#include <llarp/link.h>
#include <llarp/mem.h>

/**
 * dtls.h
 *
 * Datagram TLS functions
 * https://en.wikipedia.org/wiki/Datagram_Transport_Layer_Security for more info
 * on DTLS
 */

#ifdef __cplusplus
extern "C" {
#endif

/// DTLS configuration
struct llarp_dtls_args
{
  struct llarp_alloc* mem;
  const char* keyfile;
  const char* certfile;
};

/// allocator
void
dtls_link_init(struct llarp_link* link, struct llarp_dtls_args args,
               struct llarp_msg_muxer* muxer);

#ifdef __cplusplus
}
#endif
#endif
