#include <llarp/bencode.h>
#include <llarp/router_contact.h>
#include <llarp/version.h>

void llarp_rc_free(struct llarp_rc *rc) {
  llarp_xi_list_free(&rc->exits);
  llarp_ai_list_free(&rc->addrs);
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
