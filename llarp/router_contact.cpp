#include "router_contact.hpp"
#include "address_info.hpp"
#include "bencode.hpp"
#include "exit_info.hpp"

namespace llarp {
bool BEncode(const llarp_rc &a, llarp_buffer_t *buff) {
  std::list<llarp_ai> addresses = ai_list_to_std(a.addrs);
  std::list<llarp_xi> exits = xi_list_to_std(a.exits);
  return bencodeDict(buff) && bencodeDictKey(buff, "a") &&
         BEncode(addresses, buff) &&
         bencodeDict_Bytes(buff, "k", a.pubkey, sizeof(a.pubkey)) &&
         bencodeDict_Int(buff, "v", 0) && bencodeDictKey(buff, "x") &&
         BEncode(exits, buff) &&
         bencodeDict_Bytes(buff, "z", a.signature, sizeof(a.signature)) &&
         bencodeEnd(buff);
}
}  // namespace llarp



extern "C" {
bool llarp_rc_bencode(struct llarp_rc *rc, llarp_buffer_t *buff) {
  return llarp::BEncode(*rc, buff);
}
}
