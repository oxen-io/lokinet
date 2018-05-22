#include <arpa/inet.h>
#include <llarp/bencode.h>
#include <llarp/exit_info.h>
#include <llarp/mem.h>
#include <llarp/string.h>

struct llarp_xi_list_node
{
  struct llarp_xi data;
  struct llarp_xi_list_node *next;
};

struct llarp_xi_list
{
  struct llarp_alloc *mem;
  struct llarp_xi_list_node *root;
};

struct llarp_xi_list *
llarp_xi_list_new(struct llarp_alloc *mem)
{
  struct llarp_xi_list *l = mem->alloc(mem, sizeof(struct llarp_xi_list), 8);
  if(l)
  {
    l->mem  = mem;
    l->root = NULL;
  }
  return l;
}

void
llarp_xi_list_free(struct llarp_xi_list *l)
{
  if(l)
  {
    struct llarp_alloc *mem            = l->mem;
    struct llarp_xi_list_node *current = l->root;
    while(current)
    {
      struct llarp_xi_list_node *tmp = current->next;
      mem->free(mem, current);
      current = tmp;
    }
    mem->free(mem, l);
  }
}

static bool
llarp_xi_iter_bencode(struct llarp_xi_list_iter *iter, struct llarp_xi *xi)
{
  return llarp_xi_bencode(xi, iter->user);
}

bool
llarp_xi_list_bencode(struct llarp_xi_list *l, llarp_buffer_t *buff)
{
  if(!bencode_start_list(buff))
    return false;
  struct llarp_xi_list_iter xi_itr = {.user  = buff,
                                      .visit = &llarp_xi_iter_bencode};
  llarp_xi_list_iterate(l, &xi_itr);
  return bencode_end(buff);
}

void
llarp_xi_list_iterate(struct llarp_xi_list *l, struct llarp_xi_list_iter *iter)
{
  struct llarp_xi_list_node *current = l->root;
  iter->list                         = l;
  while(current)
  {
    if(!iter->visit(iter, &current->data))
      return;

    current = current->next;
  }
}

bool
llarp_xi_bencode(struct llarp_xi *xi, llarp_buffer_t *buff)
{
  char addr_buff[128] = {0};
  const char *addr;
  if(!bencode_start_dict(buff))
    return false;

  /** address */
  addr = inet_ntop(AF_INET6, &xi->address, addr_buff, sizeof(addr_buff));
  if(!addr)
    return false;
  if(!bencode_write_bytestring(buff, "a", 1))
    return false;
  if(!bencode_write_bytestring(buff, addr, strnlen(addr, sizeof(addr_buff))))
    return false;

  /** netmask */
  addr = inet_ntop(AF_INET6, &xi->netmask, addr_buff, sizeof(addr_buff));
  if(!addr)
    return false;
  if(!bencode_write_bytestring(buff, "b", 1))
    return false;
  if(!bencode_write_bytestring(buff, addr, strnlen(addr, sizeof(addr_buff))))
    return false;

  /** public key */
  if(!bencode_write_bytestring(buff, "k", 1))
    return false;
  if(!bencode_write_bytestring(buff, xi->pubkey, sizeof(llarp_pubkey_t)))
    return false;

  /** version */
  if(!bencode_write_version_entry(buff))
    return false;

  return bencode_end(buff);
}

static bool
llarp_xi_decode_dict(struct dict_reader *r, llarp_buffer_t *key)
{
  if(!key)
    return true;

  struct llarp_xi *xi = r->user;
  llarp_buffer_t strbuf;
  int64_t v;
  char tmp[128] = {0};

  // address
  if(llarp_buffer_eq(*key, "a"))
  {
    if(!bdecode_read_string(r->buffer, &strbuf))
      return false;
    if(strbuf.sz >= sizeof(tmp))
      return false;
    memcpy(tmp, strbuf.base, strbuf.sz);
    return inet_pton(AF_INET6, tmp, xi->address.s6_addr) == 1;
  }

  if(llarp_buffer_eq(*key, "b"))
  {
    if(!bdecode_read_string(r->buffer, &strbuf))
      return false;
    if(strbuf.sz >= sizeof(tmp))
      return false;
    memcpy(tmp, strbuf.base, strbuf.sz);
    return inet_pton(AF_INET6, tmp, xi->netmask.s6_addr) == 1;
  }

  if(llarp_buffer_eq(*key, "k"))
  {
    if(!bdecode_read_string(r->buffer, &strbuf))
      return false;
    if(strbuf.sz != sizeof(llarp_pubkey_t))
      return false;
    memcpy(xi->pubkey, strbuf.base, sizeof(llarp_pubkey_t));
    return true;
  }

  if(llarp_buffer_eq(*key, "v"))
  {
    if(!bdecode_read_integer(r->buffer, &v))
      return false;
    return v == LLARP_PROTO_VERSION;
  }

  return false;
}

bool
llarp_xi_bdecode(struct llarp_xi *xi, llarp_buffer_t *buf)
{
  struct dict_reader r = {.user = xi, .on_key = &llarp_xi_decode_dict};
  return bdecode_read_dict(buf, &r);
}

void
llarp_xi_list_pushback(struct llarp_xi_list *l, struct llarp_xi *xi)
{
  struct llarp_xi_list_node *cur = l->root;
  if(cur)
  {
    // go to the end of the list
    while(cur->next)
      cur = cur->next;

    cur->next = l->mem->alloc(l->mem, sizeof(struct llarp_xi_list_node), 16);
    cur       = cur->next;
  }
  else
  {
    l->root = l->mem->alloc(l->mem, sizeof(struct llarp_xi_list_node), 16);
    cur     = l->root;
  }

  llarp_xi_copy(&cur->data, xi);
  cur->next = 0;
}

void
llarp_xi_copy(struct llarp_xi *dst, struct llarp_xi *src)
{
  memcpy(dst, src, sizeof(struct llarp_xi));
}

static bool
llarp_xi_list_decode_item(struct list_reader *r, bool more)
{
  if(!more)
    return true;

  struct llarp_xi_list *l = r->user;
  struct llarp_xi xi;
  if(!llarp_xi_bdecode(&xi, r->buffer))
    return false;

  llarp_xi_list_pushback(l, &xi);
  return true;
}

bool
llarp_xi_list_bdecode(struct llarp_xi_list *l, llarp_buffer_t *buff)
{
  struct list_reader r = {
      .user    = l,
      .on_item = &llarp_xi_list_decode_item,
  };
  return bdecode_read_list(buff, &r);
}
