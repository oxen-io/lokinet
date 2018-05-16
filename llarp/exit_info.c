#include <arpa/inet.h>
#include <llarp/bencode.h>
#include <llarp/exit_info.h>
#include <llarp/mem.h>
#include <llarp/string.h>

struct llarp_xi_list_node {
  struct llarp_xi data;
  struct llarp_xi_list_node *next;
};

struct llarp_xi_list {
  struct llarp_alloc * mem;
  struct llarp_xi_list_node *root;
};

struct llarp_xi_list *llarp_xi_list_new(struct llarp_alloc * mem) {
  struct llarp_xi_list *l = mem->alloc(mem, sizeof(struct llarp_xi_list), 8);
  if (l) {
    l->mem = mem;
    l->root = NULL;
  }
  return l;
}

void llarp_xi_list_free(struct llarp_xi_list **l) {
  if (*l) {
    struct llarp_alloc * mem = (*l)->mem;
    struct llarp_xi_list_node *current = (*l)->root;
    while (current) {
      struct llarp_xi_list_node *tmp = current->next;
      mem->free(mem, current);
      current = tmp;
    }
    mem->free(mem, *l);
    *l = NULL;
  }
}

static bool llarp_xi_iter_bencode(struct llarp_xi_list_iter *iter,
                                  struct llarp_xi *xi) {
  return llarp_xi_bencode(xi, iter->user);
}

bool llarp_xi_list_bencode(struct llarp_xi_list *l, llarp_buffer_t *buff) {
  if (!bencode_start_list(buff)) return false;
  struct llarp_xi_list_iter xi_itr = {.user = buff,
                                      .visit = &llarp_xi_iter_bencode};
  llarp_xi_list_iterate(l, &xi_itr);
  return bencode_end(buff);
}

void llarp_xi_list_iterate(struct llarp_xi_list *l,
                           struct llarp_xi_list_iter *iter) {
  struct llarp_xi_list_node *current = l->root;
  iter->list = l;
  while (current) {
    if (!iter->visit(iter, &current->data)) return;

    current = current->next;
  }
}

bool llarp_xi_bencode(struct llarp_xi *xi, llarp_buffer_t *buff) {
  char addr_buff[128] = {0};
  const char *addr;
  if (!bencode_start_dict(buff)) return false;

  /** address */
  addr = inet_ntop(AF_INET6, &xi->address, addr_buff, sizeof(addr_buff));
  if (!addr) return false;
  if (!bencode_write_bytestring(buff, "a", 1)) return false;
  if (!bencode_write_bytestring(buff, addr, strnlen(addr, sizeof(addr_buff))))
    return false;

  /** netmask */
  addr = inet_ntop(AF_INET6, &xi->netmask, addr_buff, sizeof(addr_buff));
  if (!addr) return false;
  if (!bencode_write_bytestring(buff, "b", 1)) return false;
  if (!bencode_write_bytestring(buff, addr, strnlen(addr, sizeof(addr_buff))))
    return false;

  /** public key */
  if (!bencode_write_bytestring(buff, "k", 1)) return false;
  if (!bencode_write_bytestring(buff, xi->pubkey, sizeof(llarp_pubkey_t)))
    return false;

  /** version */
  if (!bencode_write_version_entry(buff)) return false;

  return bencode_end(buff);
}


bool llarp_xi_list_bdecode(struct llarp_xi_list * l, llarp_buffer_t * buff)
{
  return false;
}
