#ifndef LLARP_SI_H
#define LLARP_SI_H
#include <llarp/crypto.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct llarp_service_info
  {
    llarp_buffer_t name;
    llarp_pubkey_t signingkey;
    llarp_buffer_t vanity;
  };

  void llarp_service_info_hash(struct llarp_service_info * si, llarp_hash_t * h);
  bool llarp_service_info_bencode(struct llarp_serivce_info * si, llarp_buffer_t * buff);
  bool llarp_service_info_bdecode(struct llarp_serivce_info * si, llarp_buffer_t  buff);
  void llarp_service_info_free(struct llarp_service_info ** si);
  
#ifdef __cplusplus
}
#endif
#endif
