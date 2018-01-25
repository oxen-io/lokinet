#ifndef LLARP_RC_H
#define LLARP_RC_H
#include <llarp/address_info.h>
#include <llarp/exit_info.h>
#include <llarp/crypto.h>
#ifdef __cplusplus
extern "C" {
#endif
  
  struct llarp_router_contact
  {
    llarp_buffer_t raw;
    struct llarp_address_info_list * addreses;
    llarp_pubkey_t pubkey;
    struct llarp_exit_info_list * exits;
    llarp_sig_t signature;
  };

  bool llarp_rc_bdecode(struct llarp_router_contact * rc, llarp_buffer_t buf);
  bool llarp_rc_bencode(struct llarp_router_contact * rc, llarp_buffer_t * buf);
  void llarp_rc_free(struct llarp_router_contact ** rc);
  bool llarp_rc_verify_sig(struct llarp_rotuer_contact * rc);

  
  
#ifdef __cplusplus
}
#endif
#endif
