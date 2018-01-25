#ifndef LLARP_AI_H
#define LLARP_AI_H
#include <llarp/crypto.h>
#include <llarp/net.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct llarp_address_info
  {
    uint16_t rank;
    llarp_pubkey_t enc_key;
    char * dialect;
    struct in6_addr ip;
    uint16_t port;
  };

  bool llarp_address_info_bencode(struct llarp_address_info * ai, llarp_buffer_t * buff);
  bool llarp_address_info_bdecode(struct llarp_address_info * ai, llarp_buffer_t  buff);
  
  struct llarp_address_info_list;
  
#ifdef __cplusplus
}
#endif
  
#endif
