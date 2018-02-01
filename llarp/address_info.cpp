#include "address_info.hpp"
#include "bencode.hpp"
#include "str.hpp"

namespace llarp {
template <>
bool BEncode(const llarp_ai &a, llarp_buffer_t *buff) {
  return bencodeDict(buff) && bencodeDict_Int(buff, "c", a.rank) &&
         bencodeDict_Bytes(buff, "e", a.enc_key, sizeof(a.enc_key)) &&
         bencodeDict_Bytes(buff, "d", a.dialect,
                           UStrLen(a.dialect, sizeof(a.dialect))) &&
         bencodeDict_Bytes(buff, "i", &a.ip, sizeof(a.ip)) &&
         bencodeDict_Int(buff, "p", a.port) && bencodeDict_Int(buff, "v", 0) &&
         bencodeEnd(buff);
}
}  // namespace llarp

extern "C" {

bool llarp_ai_bencode(struct llarp_ai *ai, llarp_buffer_t *buff) {
  return llarp::BEncode(*ai, buff);
}

void llarp_ai_list_iterate(struct llarp_ai_list *l,
                           struct llarp_ai_list_iter *itr) {
  itr->list = l;
  struct llarp_ai_list *cur = l;
  do {
    if (!itr->visit(itr, cur->data)) return;
    cur = cur->next;
  } while (cur->next);
}
}
