#include "address_info.hpp"
#include <llarp/bencode.h>
#include <cstring>
#include <arpa/inet.h>

namespace llarp {
  /*
template <>
bool BEncode(const llarp_ai &a, llarp_buffer_t *buff) {
  return bencodeDict(buff) && bencodeDict_Int(buff, "c", a.rank) &&
         bencodeDict_Bytes(buff, "d", a.dialect,
                           UStrLen(a.dialect, sizeof(a.dialect))) &&
         bencodeDict_Bytes(buff, "e", a.enc_key, sizeof(a.enc_key)) &&
         bencodeDict_Bytes(buff, "i", &a.ip, sizeof(a.ip)) &&
         bencodeDict_Int(buff, "p", a.port) && bencodeDict_Int(buff, "v", 0) &&
         bencodeEnd(buff);
}
  */
}  // namespace llarp

extern "C" {

bool llarp_ai_bencode(struct llarp_ai *ai, llarp_buffer_t *buff) {
  if(!bencode_start_dict(buff)) return false;
  /* rank */
  if(!bencode_write_bytestring(buff, "c", 1)) return false;
  if(!bencode_write_uint16(buff, ai->rank)) return false;
  /* dialect */
  if(!bencode_write_bytestring(buff, "d", 1)) return false;
  if(!bencode_write_bytestring(buff, ai->dialect, strnlen(ai->dialect, sizeof(ai->dialect)))) return false;
  /* encryption key */
  if(!bencode_write_bytestring(buff, "e", 1)) return false;
  if(!bencode_write_bytestring(buff, ai->enc_key, sizeof(llarp_pubkey_t))) return false;
  /** ip */
  char ipbuff [128] = {0};
  const char * ipstr = inet_ntop(AF_INET6, &ai->ip, ipbuff, sizeof(ipbuff));
  if(ipstr == nullptr) return false;
  if(!bencode_write_bytestring(buff, "i", 1)) return false;
  if(!bencode_write_bytestring(buff, ipstr, strnlen(ipstr, sizeof(ipbuff)))) return false;
  /** port */
  if(!bencode_write_bytestring(buff, "p", 1)) return false;
  if(!bencode_write_uint16(buff, ai->port)) return false;

  /** version */
  if(!bencode_write_version_entry(buff)) return false;
  /** end */
  return bencode_end(buff);
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
