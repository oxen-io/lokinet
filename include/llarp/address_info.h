#ifndef LLARP_AI_H
#define LLARP_AI_H
#include <llarp/crypto.h>
#include <llarp/mem.h>
#include <llarp/net.h>
#include <stdbool.h>

/**
 * address_info.h
 *
 * utilities for handling addresses on the llarp network
 */

#define MAX_AI_DIALECT_SIZE 5

/// address information model
struct llarp_ai
{
  uint16_t rank;
  char dialect[MAX_AI_DIALECT_SIZE + 1];
  byte_t enc_key[PUBKEYSIZE];
  struct in6_addr ip;
  uint16_t port;
};

/// convert address information struct to bencoded buffer
bool
llarp_ai_bencode(struct llarp_ai *ai, llarp_buffer_t *buff);

/// convert bencoded buffer to address information struct
bool
llarp_ai_bdecode(struct llarp_ai *ai, llarp_buffer_t *buff);

struct llarp_ai_list;

/// list of address information initialization
struct llarp_ai_list *
llarp_ai_list_new();

/// list of address information destruction
void
llarp_ai_list_free(struct llarp_ai_list *l);

/// copy AI
void
llarp_ai_copy(struct llarp_ai *dst, struct llarp_ai *src);

/// convert llarp_ai_list struct to bencoded buffer
bool
llarp_ai_list_bencode(struct llarp_ai_list *l, llarp_buffer_t *buff);

/// convert bencoded buffer to llarp_ai_list struct
bool
llarp_ai_list_bdecode(struct llarp_ai_list *l, llarp_buffer_t *buff);

/// return and remove first element from ai_list
struct llarp_ai
llarp_ai_list_popfront(struct llarp_ai_list *l);

/// pushes a copy of ai to the end of the list
void
llarp_ai_list_pushback(struct llarp_ai_list *l, struct llarp_ai *ai);

/// get the number of entries in list
size_t
llarp_ai_list_size(struct llarp_ai_list *l);

void
llarp_ai_list_copy(struct llarp_ai_list *dst, struct llarp_ai_list *src);

/// does this index exist in list
bool
llarp_ai_list_index(struct llarp_ai_list *l, ssize_t idx,
                    struct llarp_ai *result);

/// ai_list iterator configuration
struct llarp_ai_list_iter
{
  /// a customizable pointer to pass data to iteration functor
  void *user;
  /// set by llarp_ai_list_iterate()
  struct llarp_ai_list *list;
  /// return false to break iteration early
  bool (*visit)(struct llarp_ai_list_iter *, struct llarp_ai *);
};

/// iterator over list and call visit functor
void
llarp_ai_list_iterate(struct llarp_ai_list *l, struct llarp_ai_list_iter *iter);

#endif
