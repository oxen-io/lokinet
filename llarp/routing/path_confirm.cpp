#include <llarp/time.h>
#include <llarp/bencode.hpp>
#include <llarp/messages/path_confirm.hpp>
#include <llarp/routing/handler.hpp>

namespace llarp
{
  namespace routing
  {
    PathConfirmMessage::PathConfirmMessage() : pathLifetime(0), pathCreated(0)
    {
    }

    PathConfirmMessage::PathConfirmMessage(uint64_t lifetime)
        : pathLifetime(lifetime), pathCreated(llarp_time_now_ms())
    {
    }

    bool
    PathConfirmMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* val)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictInt("L", pathLifetime, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictInt("S", pathCreated, read, key, val))
        return false;
      return read;
    }

    bool
    PathConfirmMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "P"))
        return false;
      if(!BEncodeWriteDictInt(buf, "L", pathLifetime))
        return false;
      if(!BEncodeWriteDictInt(buf, "S", pathCreated))
        return false;
      return bencode_end(buf);
    }

    bool
    PathConfirmMessage::HandleMessage(IMessageHandler* h) const
    {
      llarp::Info("got path confirm created=", pathCreated,
                  " lifetime=", pathLifetime);
      return true;
    }

  }  // namespace routing
}  // namespace llarp