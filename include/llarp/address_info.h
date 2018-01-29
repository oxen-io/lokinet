#ifndef LLARP_AI_H
#define LLARP_AI_H
#include <llarp/crypto.h>
#include <llarp/mem.h>
#include <llarp/net.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_AI_DIALECT_SIZE 5

struct llarp_ai {
  uint16_t rank;
  llarp_pubkey_t enc_key;
  uint8_t dialect[MAX_AI_DIALECT_SIZE + 1];
  struct in6_addr ip;
  uint16_t port;
};

/** allocator for address info */
extern struct llarp_alloc *llarp_ai_alloc;

bool llarp_ai_bencode(struct llarp_ai *ai, llarp_buffer_t *buff);
bool llarp_ai_bdecode(struct llarp_ai *ai, llarp_buffer_t buff);

struct llarp_ai_list;

struct llarp_ai_list *llarp_ai_list_new();
void llarp_ai_list_free(struct llarp_ai_list *l);

struct llarp_ai llarp_ai_list_popfront(struct llarp_ai_list *l);
void llarp_ai_list_pushback(struct llarp_ai_list *l, struct llarp_ai *ai);
size_t llarp_ai_list_size(struct llarp_ai_list *l);
struct llarp_ai *llarp_ai_list_index(struct llarp_ai_list *l, ssize_t idx);

struct llarp_ai_list_iter {
  void *user;
  /** set by llarp_ai_list_iterate() */
  struct llarp_ai_list *list;
  /** return false to break iteration */
  bool (*visit)(struct llarp_ai_list_iter *, struct llarp_ai *);
};

void llarp_ai_list_iterate(struct llarp_ai_list *l,
                           struct llarp_ai_list_iter *iter);

#ifdef __cplusplus
}
#endif

#endif
