#ifndef SARP_RC_H
#define SARP_RC_H
#include <sarp/address_info.h>
#include <sarp/exit_info.h>
#include <sarp/crypto.h>
#ifdef __cplusplus
extern "C" {
#endif
  
  struct sarp_router_contact
  {
    sarp_buffer_t raw;
    struct sarp_address_info_list * addreses;
    sarp_pubkey_t pubkey;
    struct sarp_exit_info_list * exits;
    sarp_sig_t signature;
  };

  bool sarp_rc_bdecode(struct sarp_router_contact * rc, sarp_buffer_t buf);
  bool sarp_rc_bencode(struct sarp_router_contact * rc, sarp_buffer_t * buf);
  void sarp_rc_free(struct sarp_router_contact ** rc);
  bool sarp_rc_verify_sig(struct sarp_rotuer_contact * rc);

  
  
#ifdef __cplusplus
}
#endif
#endif
