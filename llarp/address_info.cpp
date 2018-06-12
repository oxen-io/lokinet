#include <arpa/inet.h>
#include <llarp/address_info.h>
#include <llarp/bencode.h>
#include <llarp/mem.h>
#include <llarp/string.h>
#include <vector>

struct llarp_ai_list
{
  std::vector< llarp_ai > list;
};

static bool
llarp_ai_decode_key(struct dict_reader *r, llarp_buffer_t *key)
{
  uint64_t i;
  char tmp[128] = {0};

  llarp_buffer_t strbuf;
  llarp_ai *ai = static_cast< llarp_ai * >(r->user);

  // done
  if(!key)
    return true;

  // rank
  if(llarp_buffer_eq(*key, "c"))
  {
    if(!bencode_read_integer(r->buffer, &i))
      return false;

    if(i > 65536 || i <= 0)
      return false;

    ai->rank = i;
    return true;
  }

  // dialect
  if(llarp_buffer_eq(*key, "d"))
  {
    if(!bencode_read_string(r->buffer, &strbuf))
      return false;

    if(strbuf.sz >= sizeof(ai->dialect))
      return false;

    memcpy(ai->dialect, strbuf.base, strbuf.sz);
    ai->dialect[strbuf.sz] = 0;
    return true;
  }

  // encryption public key
  if(llarp_buffer_eq(*key, "e"))
  {
    if(!bencode_read_string(r->buffer, &strbuf))
      return false;

    if(strbuf.sz != PUBKEYSIZE)
      return false;

    memcpy(ai->enc_key, strbuf.base, PUBKEYSIZE);
    return true;
  }

  // ip address
  if(llarp_buffer_eq(*key, "i"))
  {
    if(!bencode_read_string(r->buffer, &strbuf))
      return false;

    if(strbuf.sz >= sizeof(tmp))
      return false;

    memcpy(tmp, strbuf.base, strbuf.sz);
    tmp[strbuf.sz] = 0;
    return inet_pton(AF_INET6, tmp, &ai->ip.s6_addr[0]) == 1;
  }

  // port
  if(llarp_buffer_eq(*key, "p"))
  {
    if(!bencode_read_integer(r->buffer, &i))
      return false;

    if(i > 65536 || i <= 0)
      return false;

    ai->port = i;
    return true;
  }

  // version
  if(llarp_buffer_eq(*key, "v"))
  {
    if(!bencode_read_integer(r->buffer, &i))
      return false;
    return i == LLARP_PROTO_VERSION;
  }

  // bad key
  return false;
}

static bool
llarp_ai_list_bdecode_item(struct list_reader *r, bool more)
{
  if(!more)
    return true;
  llarp_ai_list *l = static_cast< llarp_ai_list * >(r->user);
  llarp_ai ai;

  if(!llarp_ai_bdecode(&ai, r->buffer))
    return false;

  llarp_ai_list_pushback(l, &ai);
  return true;
}

static bool
llarp_ai_list_iter_bencode(struct llarp_ai_list_iter *iter, struct llarp_ai *ai)
{
  return llarp_ai_bencode(ai, static_cast< llarp_buffer_t * >(iter->user));
}

extern "C" {

bool
llarp_ai_bdecode(struct llarp_ai *ai, llarp_buffer_t *buff)
{
  struct dict_reader reader = {
      .buffer = nullptr, .user = ai, .on_key = &llarp_ai_decode_key};
  return bencode_read_dict(buff, &reader);
}

bool
llarp_ai_bencode(struct llarp_ai *ai, llarp_buffer_t *buff)
{
  char ipbuff[128] = {0};
  const char *ipstr;
  if(!bencode_start_dict(buff))
    return false;
  /* rank */
  if(!bencode_write_bytestring(buff, "c", 1))
    return false;
  if(!bencode_write_uint16(buff, ai->rank))
    return false;
  /* dialect */
  if(!bencode_write_bytestring(buff, "d", 1))
    return false;
  if(!bencode_write_bytestring(buff, ai->dialect,
                               strnlen(ai->dialect, sizeof(ai->dialect))))
    return false;
  /* encryption key */
  if(!bencode_write_bytestring(buff, "e", 1))
    return false;
  if(!bencode_write_bytestring(buff, ai->enc_key, PUBKEYSIZE))
    return false;
  /** ip */
  ipstr = inet_ntop(AF_INET6, &ai->ip, ipbuff, sizeof(ipbuff));
  if(!ipstr)
    return false;
  if(!bencode_write_bytestring(buff, "i", 1))
    return false;
  if(!bencode_write_bytestring(buff, ipstr, strnlen(ipstr, sizeof(ipbuff))))
    return false;
  /** port */
  if(!bencode_write_bytestring(buff, "p", 1))
    return false;
  if(!bencode_write_uint16(buff, ai->port))
    return false;

  /** version */
  if(!bencode_write_version_entry(buff))
    return false;
  /** end */
  return bencode_end(buff);
}

bool
llarp_ai_list_bencode(struct llarp_ai_list *l, llarp_buffer_t *buff)
{
  if(!bencode_start_list(buff))
    return false;
  struct llarp_ai_list_iter ai_itr = {
      .user = buff, .list = nullptr, .visit = &llarp_ai_list_iter_bencode};
  llarp_ai_list_iterate(l, &ai_itr);
  return bencode_end(buff);
}

struct llarp_ai_list *
llarp_ai_list_new()
{
  return new llarp_ai_list;
}

void
llarp_ai_list_free(struct llarp_ai_list *l)
{
  if(l)
  {
    l->list.clear();
    delete l;
  }
}

void
llarp_ai_copy(struct llarp_ai *dst, struct llarp_ai *src)
{
  memcpy(dst, src, sizeof(struct llarp_ai));
}

void
llarp_ai_list_copy(struct llarp_ai_list *dst, struct llarp_ai_list *src)
{
  dst->list.clear();
  for(auto &itr : src->list)
    dst->list.emplace_back(itr);
}

void
llarp_ai_list_pushback(struct llarp_ai_list *l, struct llarp_ai *ai)
{
  l->list.push_back(*ai);
}

void
llarp_ai_list_iterate(struct llarp_ai_list *l, struct llarp_ai_list_iter *itr)
{
  itr->list = l;
  for(auto &ai : l->list)
    if(!itr->visit(itr, &ai))
      return;
}

bool
llarp_ai_list_index(struct llarp_ai_list *l, ssize_t idx, struct llarp_ai *dst)
{
  // TODO: implement negative indexes
  if(idx < 0)
    return false;

  size_t i = idx;

  if(l->list.size() > i)
  {
    llarp_ai_copy(dst, &l->list[i]);
    return true;
  }
  return false;
}

bool
llarp_ai_list_bdecode(struct llarp_ai_list *l, llarp_buffer_t *buff)
{
  struct list_reader r = {
      .buffer = nullptr, .user = l, .on_item = &llarp_ai_list_bdecode_item};
  return bencode_read_list(buff, &r);
}
}
