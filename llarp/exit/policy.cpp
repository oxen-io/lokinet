#include "policy.hpp"

namespace llarp
{
  namespace exit
  {
    bool
    Policy::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictInt("a", proto, buf))
        return false;
      if (!BEncodeWriteDictInt("b", port, buf))
        return false;
      if (!BEncodeWriteDictInt("d", drop, buf))
        return false;
      if (!BEncodeWriteDictInt("v", version, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    Policy::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictInt("a", proto, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("b", port, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("d", drop, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("v", version, read, k, buf))
        return false;
      return read;
    }

  }  // namespace exit
}  // namespace llarp
