#include "router_contact.hpp"
#include <llarp/bencode.h>
#include <llarp/version.h>
#include "address_info.hpp"
#include "exit_info.hpp"

namespace llarp {

static bool bencode_rc_ai_write(struct llarp_ai_list_iter *i,
                                struct llarp_ai *ai) {
  llarp_buffer_t *buff = static_cast<llarp_buffer_t *>(i->user);
  return llarp_ai_bencode(ai, buff);
}

static bool bencode_rc_xi_write(struct llarp_xi_list_iter *i,
                                struct llarp_xi *xi) {
  llarp_buffer_t *buff = static_cast<llarp_buffer_t *>(i->user);
  return llarp_xi_bencode(xi, buff);
}
}  // namespace llarp

extern "C" {

  void llarp_rc_free(struct llarp_rc *rc)
  {
    if(rc->exits)
      llarp_xi_list_free(rc->exits);
    if(rc->addrs)
      llarp_ai_list_free(rc->addrs);
  }
  
bool llarp_rc_bencode(struct llarp_rc *rc, llarp_buffer_t *buff) {
  /* write dict begin */
  if (!bencode_start_dict(buff)) return false;
  if (rc->addrs) {
    /* write ai if they exist */
    if (!bencode_write_bytestring(buff, "a", 1)) return false;
    if (!bencode_start_list(buff)) return false;
    struct llarp_ai_list_iter ai_itr = {
        .user = buff, .list = nullptr, .visit = &llarp::bencode_rc_ai_write};
    llarp_ai_list_iterate(rc->addrs, &ai_itr);
    if (!bencode_end(buff)) return false;
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
    if (!bencode_start_list(buff)) return false;
    struct llarp_xi_list_iter xi_itr = {
        .user = buff, .list = nullptr, .visit = &llarp::bencode_rc_xi_write};
    llarp_xi_list_iterate(rc->exits, &xi_itr);
    if (!bencode_end(buff)) return false;
  }

  /* write signature */
  if (!bencode_write_bytestring(buff, "z", 1)) return false;
  if (!bencode_write_bytestring(buff, rc->signature, sizeof(llarp_sig_t)))
    return false;
  return bencode_end(buff);
}
}
