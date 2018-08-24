#ifndef LLARP_XI_H
#define LLARP_XI_H
#include <llarp/buffer.h>
#include <llarp/crypto.h>
#include <llarp/net.h>
#ifdef __cplusplus
#include <iostream>
#include <llarp/bits.hpp>
#endif

/**
 * exit_info.h
 *
 * utilities for handling exits on the llarp network
 */

struct llarp_xi;

bool
llarp_xi_bdecode(struct llarp_xi *xi, llarp_buffer_t *buf);
bool
llarp_xi_bencode(const struct llarp_xi *xi, llarp_buffer_t *buf);

/// Exit info model
struct llarp_xi
{
  struct in6_addr address;
  struct in6_addr netmask;
  byte_t pubkey[PUBKEYSIZE];

#ifdef __cplusplus
  bool
  BEncode(llarp_buffer_t *buf) const
  {
    return llarp_xi_bencode(this, buf);
  }

  bool
  BDecode(llarp_buffer_t *buf)
  {
    return llarp_xi_bdecode(this, buf);
  }

  friend std::ostream &
  operator<<(std::ostream &out, const llarp_xi &xi)
  {
    char tmp[128] = {0};
    if(inet_ntop(AF_INET6, &xi.address, tmp, sizeof(tmp)))
      out << std::string(tmp);
    else
      return out;
    out << std::string("/");
    return out << std::to_string(
               llarp::bits::count_array_bits(xi.netmask.s6_addr));
  }
#endif
};

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

size_t
llarp_xi_list_size(const struct llarp_xi_list *l);

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
