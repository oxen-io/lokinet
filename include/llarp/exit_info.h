#ifndef LLARP_XI_H
#define LLARP_XI_H
#include <llarp/buffer.h>
#include <llarp/crypto.h>
#include <llarp/net.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_xi {
  struct in6_addr address;
  struct in6_addr netmask;
  llarp_pubkey_t pubkey;
};

bool llarp_xi_bdecode(struct llarp_xi *xi, llarp_buffer_t *buf);
bool llarp_xi_bencode(struct llarp_xi *xi, llarp_buffer_t *buf);

struct llarp_xi_list;

  struct llarp_xi_list * llarp_xi_list_new();
  void llarp_xi_list_free(struct llarp_xi_list * l);


  struct llarp_xi_list_iter
  {
    void * user;
    struct llarp_xi_list * list;
    bool (*visit)(struct llarp_xi_list_iter *, struct llarp_xi *);
  };

  void llarp_xi_list_iterate(struct llarp_xi_list *l,
                             struct llarp_xi_list_iter *iter);

#ifdef __cplusplus
}
#endif
#endif
