#include <arpa/inet.h>
#include <llarp/address_info.h>
#include <llarp/bencode.h>
#include <llarp/mem.h>
#include <llarp/string.h>

bool llarp_ai_bdecode(struct llarp_ai *ai, llarp_buffer_t *buff)
{

}

bool llarp_ai_bencode(struct llarp_ai *ai, llarp_buffer_t *buff) {
  char ipbuff[128] = {0};
  const char *ipstr;
  if (!bencode_start_dict(buff)) return false;
  /* rank */
  if (!bencode_write_bytestring(buff, "c", 1)) return false;
  if (!bencode_write_uint16(buff, ai->rank)) return false;
  /* dialect */
  if (!bencode_write_bytestring(buff, "d", 1)) return false;
  if (!bencode_write_bytestring(buff, ai->dialect,
                                strnlen(ai->dialect, sizeof(ai->dialect))))
    return false;
  /* encryption key */
  if (!bencode_write_bytestring(buff, "e", 1)) return false;
  if (!bencode_write_bytestring(buff, ai->enc_key, sizeof(llarp_pubkey_t)))
    return false;
  /** ip */
  ipstr = inet_ntop(AF_INET6, &ai->ip, ipbuff, sizeof(ipbuff));
  if (!ipstr) return false;
  if (!bencode_write_bytestring(buff, "i", 1)) return false;
  if (!bencode_write_bytestring(buff, ipstr, strnlen(ipstr, sizeof(ipbuff))))
    return false;
  /** port */
  if (!bencode_write_bytestring(buff, "p", 1)) return false;
  if (!bencode_write_uint16(buff, ai->port)) return false;

  /** version */
  if (!bencode_write_version_entry(buff)) return false;
  /** end */
  return bencode_end(buff);
}

static bool llarp_ai_list_iter_bencode(struct llarp_ai_list_iter *iter,
                                       struct llarp_ai *ai) {
  return llarp_ai_bencode(ai, iter->user);
}

bool llarp_ai_list_bencode(struct llarp_ai_list *l, llarp_buffer_t *buff) {
  if (!bencode_start_list(buff)) return false;
  struct llarp_ai_list_iter ai_itr = {.user = buff,
                                      .visit = &llarp_ai_list_iter_bencode};
  llarp_ai_list_iterate(l, &ai_itr);
  return bencode_end(buff);
}

struct llarp_ai_list_node {
  struct llarp_ai data;
  struct llarp_ai_list_node *next;
};

struct llarp_ai_list {
  struct llarp_ai_list_node *root;
};

struct llarp_ai_list *llarp_ai_list_new() {
  struct llarp_ai_list *l = llarp_g_mem.alloc(sizeof(struct llarp_ai_list), 8);
  if (l) {
    l->root = NULL;
  }
  return l;
}

void llarp_ai_list_free(struct llarp_ai_list **l) {
  if (*l) {
    struct llarp_ai_list_node *cur = (*l)->root;
    while (cur) {
      struct llarp_ai_list_node *tmp = cur->next;
      llarp_g_mem.free(cur);
      cur = tmp;
    }
    llarp_g_mem.free(*l);
    *l = NULL;
  }
}

void llarp_ai_list_iterate(struct llarp_ai_list *l,
                           struct llarp_ai_list_iter *itr) {
  struct llarp_ai_list_node *cur = l->root;
  itr->list = l;
  while (cur) {
    if (!itr->visit(itr, &cur->data)) return;
    cur = cur->next;
  };
}

bool llarp_ai_list_bdecode(struct llarp_ai_list * l, llarp_buffer_t * buff)
{
  return false;
}
