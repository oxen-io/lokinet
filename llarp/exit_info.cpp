#include "exit_info.hpp"
#include "bencode.hpp"

namespace llarp {
template <>
bool BEncode(const struct llarp_xi &xi, llarp_buffer_t *buff) {
  return bencodeDict(buff) &&
         bencodeDict_Bytes(buff, "a", &xi.address, sizeof(xi.address)) &&
         bencodeDict_Bytes(buff, "b", &xi.netmask, sizeof(xi.netmask)) &&
         bencodeDict_Int(buff, "v", 0) && bencodeEnd(buff);
}
}  // namespace llarp

extern "C" {
bool llarp_xi_bencode(struct llarp_xi *xi, llarp_buffer_t *buff) {
  return llarp::BEncode(*xi, buff);
}
}
