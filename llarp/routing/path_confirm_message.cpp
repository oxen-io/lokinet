#include "path_confirm_message.hpp"

#include "handler.hpp"
#include <llarp/util/bencode.hpp>
#include <llarp/util/time.hpp>

namespace llarp
{
  namespace routing
  {
    PathConfirmMessage::PathConfirmMessage(llarp_time_t lifetime)
        : pathLifetime(lifetime), pathCreated(time_now_ms())
    {}

    bool
    PathConfirmMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictInt("L", pathLifetime, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictInt("S", S, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictInt("T", pathCreated, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictInt("V", version, read, key, val))
        return false;
      return read;
    }

    bool
    PathConfirmMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictMsgType(buf, "A", "P"))
        return false;
      if (!BEncodeWriteDictInt("L", pathLifetime.count(), buf))
        return false;
      if (!BEncodeWriteDictInt("S", S, buf))
        return false;
      if (!BEncodeWriteDictInt("T", pathCreated.count(), buf))
        return false;
      if (!BEncodeWriteDictInt("V", version, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    PathConfirmMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      return h && h->HandlePathConfirmMessage(*this, r);
    }

  }  // namespace routing
}  // namespace llarp
