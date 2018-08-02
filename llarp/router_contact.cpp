#include <llarp/bencode.h>
#include <llarp/router_contact.h>
#include <llarp/version.h>
#include <llarp/crypto.hpp>
#include "buffer.hpp"
#include "logger.hpp"
#include "mem.hpp"

bool
llarp_rc_new(struct llarp_rc *rc)
{
  rc->addrs        = llarp_ai_list_new();
  rc->exits        = llarp_xi_list_new();
  rc->last_updated = 0;
  llarp::Zero(rc->nickname, sizeof(rc->nickname));
  return true;
}

void
llarp_rc_free(struct llarp_rc *rc)
{
  if(rc->exits)
    llarp_xi_list_free(rc->exits);
  if(rc->addrs)
    llarp_ai_list_free(rc->addrs);

  rc->exits = 0;
  rc->addrs = 0;
}

struct llarp_rc_decoder
{
  struct llarp_rc *rc;
  struct llarp_alloc *mem;
};

static bool
llarp_rc_decode_dict(struct dict_reader *r, llarp_buffer_t *key)
{
  uint64_t v;
  llarp_buffer_t strbuf;
  llarp_rc *rc = static_cast< llarp_rc * >(r->user);

  if(!key)
    return true;

  if(llarp_buffer_eq(*key, "a"))
  {
    if(rc->addrs)
    {
      llarp_ai_list_free(rc->addrs);
    }
    rc->addrs = llarp_ai_list_new();
    return llarp_ai_list_bdecode(rc->addrs, r->buffer);
  }

  if(llarp_buffer_eq(*key, "k"))
  {
    if(!bencode_read_string(r->buffer, &strbuf))
      return false;
    if(strbuf.sz != PUBKEYSIZE)
      return false;
    memcpy(rc->pubkey, strbuf.base, PUBKEYSIZE);
    return true;
  }

  if(llarp_buffer_eq(*key, "n"))
  {
    if(!bencode_read_string(r->buffer, &strbuf))
      return false;
    if(strbuf.sz < sizeof(rc->nickname))
      return false;
    llarp::Zero(rc->nickname, sizeof(rc->nickname));
    memcpy(rc->nickname, strbuf.base, strbuf.sz);
    return true;
  }

  if(llarp_buffer_eq(*key, "p"))
  {
    if(!bencode_read_string(r->buffer, &strbuf))
      return false;
    if(strbuf.sz != PUBKEYSIZE)
      return false;
    memcpy(rc->enckey, strbuf.base, PUBKEYSIZE);
    return true;
  }

  if(llarp_buffer_eq(*key, "u"))
  {
    if(!bencode_read_integer(r->buffer, &rc->last_updated))
      return false;
    return true;
  }

  if(llarp_buffer_eq(*key, "v"))
  {
    if(!bencode_read_integer(r->buffer, &v))
      return false;
    return v == LLARP_PROTO_VERSION;
  }

  if(llarp_buffer_eq(*key, "x"))
  {
    if(rc->exits)
    {
      llarp_xi_list_free(rc->exits);
    }
    rc->exits = llarp_xi_list_new();
    return llarp_xi_list_bdecode(rc->exits, r->buffer);
  }

  if(llarp_buffer_eq(*key, "z"))
  {
    if(!bencode_read_string(r->buffer, &strbuf))
      return false;
    if(strbuf.sz != SIGSIZE)
      return false;
    memcpy(rc->signature, strbuf.base, SIGSIZE);
    return true;
  }

  return false;
}

bool
llarp_rc_is_public_router(const struct llarp_rc *const rc)
{
  return rc->addrs && llarp_ai_list_size(rc->addrs) > 0;
}

bool
llarp_rc_set_nickname(struct llarp_rc *rc, const char *nick)
{
  strncpy((char *)rc->nickname, nick, sizeof(rc->nickname));
  /// TODO: report nickname truncation
  return true;
}

void
llarp_rc_copy(struct llarp_rc *dst, const struct llarp_rc *src)
{
  llarp_rc_free(dst);
  llarp_rc_clear(dst);
  memcpy(dst->pubkey, src->pubkey, PUBKEYSIZE);
  memcpy(dst->enckey, src->enckey, PUBKEYSIZE);
  memcpy(dst->signature, src->signature, SIGSIZE);
  dst->last_updated = src->last_updated;

  if(src->addrs)
  {
    dst->addrs = llarp_ai_list_new();
    llarp_ai_list_copy(dst->addrs, src->addrs);
  }
  if(src->exits)
  {
    dst->exits = llarp_xi_list_new();
    llarp_xi_list_copy(dst->exits, src->exits);
  }
  memcpy(dst->nickname, src->nickname, sizeof(dst->nickname));
}

bool
llarp_rc_bdecode(struct llarp_rc *rc, llarp_buffer_t *buff)
{
  dict_reader r = {buff, rc, &llarp_rc_decode_dict};
  return bencode_read_dict(buff, &r);
}

bool
llarp_rc_verify_sig(struct llarp_crypto *crypto, struct llarp_rc *rc)
{
  // maybe we should copy rc before modifying it
  // would that make it more thread safe?
  // jeff agrees
  bool result = false;
  llarp::Signature sig;
  byte_t tmp[MAX_RC_SIZE];

  auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
  // copy sig
  memcpy(sig, rc->signature, SIGSIZE);
  // zero sig
  size_t sz = 0;
  while(sz < SIGSIZE)
    rc->signature[sz++] = 0;

  // bencode
  if(llarp_rc_bencode(rc, &buf))
  {
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    result  = crypto->verify(rc->pubkey, buf, sig);
  }
  else
    llarp::LogWarn("RC encode failed");
  // restore sig
  memcpy(rc->signature, sig, SIGSIZE);
  return result;
}

bool
llarp_rc_bencode(const struct llarp_rc *rc, llarp_buffer_t *buff)
{
  /* write dict begin */
  if(!bencode_start_dict(buff))
    return false;

  if(rc->addrs)
  {
    /* write ai if they exist */
    if(!bencode_write_bytestring(buff, "a", 1))
      return false;
    if(!llarp_ai_list_bencode(rc->addrs, buff))
      return false;
  }

  /* write signing pubkey */
  if(!bencode_write_bytestring(buff, "k", 1))
    return false;
  if(!bencode_write_bytestring(buff, rc->pubkey, PUBKEYSIZE))
    return false;

  /* write nickname */
  if(!bencode_write_bytestring(buff, "n", 1))
    return false;
  if(!bencode_write_bytestring(
         buff, rc->nickname,
         strnlen((char *)rc->nickname, sizeof(rc->nickname))))
    return false;

  /* write encryption pubkey */
  if(!bencode_write_bytestring(buff, "p", 1))
    return false;
  if(!bencode_write_bytestring(buff, rc->enckey, PUBKEYSIZE))
    return false;

  /* write last updated */
  if(!bencode_write_bytestring(buff, "u", 1))
    return false;
  if(!bencode_write_uint64(buff, rc->last_updated))
    return false;

  /* write version */
  if(!bencode_write_version_entry(buff))
    return false;

  if(rc->exits)
  {
    /* write ai if they exist */
    if(!bencode_write_bytestring(buff, "x", 1))
      return false;
    if(!llarp_xi_list_bencode(rc->exits, buff))
      return false;
  }

  /* write signature */
  if(!bencode_write_bytestring(buff, "z", 1))
    return false;
  if(!bencode_write_bytestring(buff, rc->signature, SIGSIZE))
    return false;
  return bencode_end(buff);
}
