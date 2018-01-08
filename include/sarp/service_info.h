#ifndef SARP_SI_H
#define SARP_SI_H
#include <sarp/crypto.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct sarp_service_info
  {
    sarp_buffer_t name;
    sarp_pubkey_t signingkey;
    sarp_buffer_t vanity;
  };

  void sarp_service_info_hash(struct sarp_service_info * si, sarp_hash_t * h);
  bool sarp_service_info_bencode(struct sarp_serivce_info * si, sarp_buffer_t * buff);
  bool sarp_service_info_bdecode(struct sarp_serivce_info * si, sarp_buffer_t  buff);
  void sarp_service_info_free(struct sarp_service_info ** si);
  
#ifdef __cplusplus
}
#endif
#endif
