#pragma once

#include <llarp/util/bencode.hpp>

namespace llarp
{
  namespace exit
  {
    struct Policy
    {
      uint64_t proto = 0;
      uint64_t port = 0;
      uint64_t drop = 0;
      uint64_t version = LLARP_PROTO_VERSION;

      bool
      BDecode(llarp_buffer_t* buf)
      {
        return bencode_decode_dict(*this, buf);
      }

      bool
      DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* val);

      bool
      BEncode(llarp_buffer_t* buf) const;
    };
  }  // namespace exit
}  // namespace llarp
