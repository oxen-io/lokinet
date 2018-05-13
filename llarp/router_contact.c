#include <llarp/bencode.h>
#include <llarp/router_contact.h>
#include <llarp/version.h>

void llarp_rc_free(struct llarp_rc *rc) {
  llarp_xi_list_free(&rc->exits);
  llarp_ai_list_free(&rc->addrs);
}


static bool llarp_rc_decode_dict(struct dict_reader * r, llarp_buffer_t * key)
{
  int64_t v;
  llarp_buffer_t strbuf;
  struct llarp_rc * rc = r->user;

  if(!key) return true;

  if(llarp_buffer_eq(*key, "a"))
  {
    rc->addrs = llarp_ai_list_new();
    return llarp_ai_list_bdecode(rc->addrs, r->buffer);
  }

  if(llarp_buffer_eq(*key, "k"))
  {
    if(!bdecode_read_string(r->buffer, &strbuf)) 
      return false;
    if(strbuf.sz != sizeof(llarp_pubkey_t)) 
      return false;
    memcpy(rc->pubkey, strbuf.base, sizeof(llarp_pubkey_t));
    return true;
  } 

  if(llarp_buffer_eq(*key, "v"))
  {
    if(!bdecode_read_integer(r->buffer, &v)) 
      return false;
    return v == LLARP_PROTO_VERSION;
  }

  if(llarp_buffer_eq(*key, "x"))
  {
    rc->exits = llarp_xi_list_new();
    return llarp_xi_list_bdecode(rc->exits, r->buffer);
  }

  if(llarp_buffer_eq(*key, "z"))
  {
    if(!bdecode_read_string(r->buffer, &strbuf))
      return false;
    if(strbuf.sz != sizeof(llarp_sig_t))
      return false;
    memcpy(rc->signature, strbuf.base, sizeof(llarp_sig_t));
    return true;
  }

  return false;
}

bool llarp_rc_bdecode(struct llarp_rc * rc, llarp_buffer_t *buff) {
  struct dict_reader r = {
    .user = rc,
    .on_key = &llarp_rc_decode_dict
  };
  return bdecode_read_dict(buff, &r);
}

bool llarp_rc_verify_sig(struct llarp_rc * rc)
{
  return false;
}

bool llarp_rc_bencode(struct llarp_rc *rc, llarp_buffer_t *buff) {
  /* write dict begin */
  if (!bencode_start_dict(buff)) return false;
  if (rc->addrs) {
    /* write ai if they exist */
    if (!bencode_write_bytestring(buff, "a", 1)) return false;
    if (!llarp_ai_list_bencode(rc->addrs, buff)) return false;
  }
  /* write pubkey */
  if (!bencode_write_bytestring(buff, "k", 1)) return false;
  if (!bencode_write_bytestring(buff, rc->pubkey, sizeof(llarp_pubkey_t)))
    return false;

  /* write version */
  if (!bencode_write_version_entry(buff)) return false;

  if (rc->exits) {
    /* write ai if they exist */
    if (!bencode_write_bytestring(buff, "x", 1)) return false;
    if (!llarp_xi_list_bencode(rc->exits, buff)) return false;
  }

  /* write signature */
  if (!bencode_write_bytestring(buff, "z", 1)) return false;
  if (!bencode_write_bytestring(buff, rc->signature, sizeof(llarp_sig_t)))
    return false;
  return bencode_end(buff);
}
