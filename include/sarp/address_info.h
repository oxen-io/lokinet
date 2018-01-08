#ifndef SARP_AI_H
#define SARP_AI_H
#include <sarp/crypto.h>
#include <sarp/net.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct sarp_address_info
  {
    uint16_t rank;
    sarp_pubkey_t enc_key;
    char * dialect;
    struct in6_addr ip;
    uint16_t port;
  };

  bool sarp_address_info_bencode(struct sarp_address_info * ai, sarp_buffer_t * buff);
  bool sarp_address_info_bdecode(struct sarp_address_info * ai, sarp_buffer_t  buff);
  
  struct sarp_address_info_list;
  
#ifdef __cplusplus
}
#endif
  
#endif
