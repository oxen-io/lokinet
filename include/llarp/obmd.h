#ifndef LLARP_OBMD_H_
#define LLARP_OBMD_H_
#include <llarp/buffer.h>
#include <llarp/crypto.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// forward declair
struct llarp_link;

struct llarp_link_dispatcher;

struct llarp_link_dispatcher *llarp_init_link_dispatcher();
void llarp_free_link_dispatcher(struct llarp_link_dispatcher **dispatcher);

void llarp_link_sendto(struct llarp_link_dispatcher *dispatcher,
                       llarp_pubkey_t pubkey, llarp_buffer_t msg);

void llarp_link_register(struct llarp_link_dispatcher *dispatcher,
                         struct llarp_link *link);

#ifdef __cplusplus
}
#endif

#endif
