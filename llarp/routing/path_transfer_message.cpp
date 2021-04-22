#include "path_transfer_message.hpp"

#include "handler.hpp"
#include <llarp/util/buffer.hpp>

namespace llarp
{
  namespace routing
  {
    bool
    PathTransferMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictEntry("P", P, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictInt("S", S, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("T", T, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictInt("V", version, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("Y", Y, read, key, val))
        return false;
      return read;
    }

    bool
    PathTransferMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictMsgType(buf, "A", "T"))
        return false;
      if (!BEncodeWriteDictEntry("P", P, buf))
        return false;

      if (!BEncodeWriteDictInt("S", S, buf))
        return false;

      if (!BEncodeWriteDictEntry("T", T, buf))
        return false;

      if (!BEncodeWriteDictInt("V", LLARP_PROTO_VERSION, buf))
        return false;
      if (!BEncodeWriteDictEntry("Y", Y, buf))
        return false;

      return bencode_end(buf);
    }

    bool
    PathTransferMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      return h->HandlePathTransferMessage(*this, r);
    }

  }  // namespace routing
}  // namespace llarp
