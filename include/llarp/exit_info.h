#ifndef LLARP_XI_H
#define LLARP_XI_H
#include <llarp/buffer.h>
#include <llarp/crypto.h>
#include <llarp/net.h>

/**
 * exit_info.h
 *
 * utilities for handling exits on the llarp network
 */

/// Exit info model
struct llarp_xi
{
  struct in6_addr address;
  struct in6_addr netmask;
  byte_t pubkey[PUBKEYSIZE];
};

bool
llarp_xi_bdecode(struct llarp_xi *xi, llarp_buffer_t *buf);
bool
llarp_xi_bencode(struct llarp_xi *xi, llarp_buffer_t *buf);

struct llarp_xi_list;

struct llarp_xi_list *
llarp_xi_list_new();

void
llarp_xi_list_free(struct llarp_xi_list *l);

bool
llarp_xi_list_bdecode(struct llarp_xi_list *l, llarp_buffer_t *buf);

bool
llarp_xi_list_bencode(struct llarp_xi_list *l, llarp_buffer_t *buf);

void
llarp_xi_list_pushback(struct llarp_xi_list *l, struct llarp_xi *xi);

void
llarp_xi_list_copy(struct llarp_xi_list *dst, struct llarp_xi_list *src);

void
llarp_xi_copy(struct llarp_xi *dst, struct llarp_xi *src);

struct llarp_xi_list_iter
{
  void *user;
  struct llarp_xi_list *list;
  bool (*visit)(struct llarp_xi_list_iter *, struct llarp_xi *);
};

void
llarp_xi_list_iterate(struct llarp_xi_list *l, struct llarp_xi_list_iter *iter);

#endif
