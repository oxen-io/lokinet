#ifndef LLARP_ROUTER_IDENT_H
#define LLARP_ROUTER_IDENT_H
#include <llarp/address_info.h>
#include <llarp/router_contact.h>
#include <llarp/crypto.h>
#include <llarp/exit_info.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

/** 
 * rotuer identity private info 
 */
struct llarp_router_ident
{
  struct llarp_crypto * crypto;
  llarp_seckey_t signkey;
  uint64_t updated;
  uint16_t version;
  struct llarp_ai_list * addrs;
  struct llarp_xi_list * exits;
};

void llarp_router_ident_new(struct llarp_router_ident ** ri, struct llarp_crypto * crypto);

void llarp_router_ident_append_ai(struct llarp_router_ident * ri, struct llarp_ai * ai);
  
bool llarp_router_ident_bdecode(struct llarp_router_ident *ri, llarp_buffer_t *buf);
bool llarp_router_ident_bencode(struct llarp_router_ident *ri, llarp_buffer_t *buf);
void llarp_router_ident_free(struct llarp_router_ident ** ri);
bool llarp_router_ident_generate_rc(struct llarp_router_ident * ri, struct llarp_rc ** rc);


#ifdef __cplusplus
}
#endif
#endif
