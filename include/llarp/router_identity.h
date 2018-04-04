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
  llarp_seckey_t signkey;
  uint64_t updated;
  uint16_t version;
  struct llarp_ai_list * addrs;
  struct llarp_xi_list * exits;
};

bool llarp_router_ident_bdecode(struct llarp_router_ident *rc, llarp_buffer_t *buf);
bool llarp_router_ident_bencode(struct llarp_router_ident *rc, llarp_buffer_t *buf);
void llarp_router_ident_free(struct llarp_router_ident **rc);
bool llarp_router_ident_gen_rc(struct llarp_router_ident * rc, llarp_buffer_t * buf);

#ifdef __cplusplus
}
#endif
#endif
