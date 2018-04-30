#ifndef LLARP_RC_H
#define LLARP_RC_H
#include <llarp/address_info.h>
#include <llarp/crypto.h>
#include <llarp/exit_info.h>
#ifdef __cplusplus
extern "C" {
#endif

struct llarp_rc {
  struct llarp_ai_list *addrs;
  llarp_pubkey_t pubkey;
  struct llarp_xi_list *exits;
  llarp_sig_t signature;
};

bool llarp_rc_bdecode(struct llarp_rc *rc, llarp_buffer_t *buf);
bool llarp_rc_bencode(struct llarp_rc *rc, llarp_buffer_t *buf);
void llarp_rc_free(struct llarp_rc *rc);
bool llarp_rc_verify_sig(struct llarp_rc *rc);

#ifdef __cplusplus
}
#endif
#endif
