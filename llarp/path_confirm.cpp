#include <llarp/time.h>
#include <llarp/bencode.hpp>
#include <llarp/messages/path_confirm.hpp>

namespace llarp
{
  namespace routing
  {
    PathConfirmMessage::PathConfirmMessage(uint64_t lifetime)
        : pathLifetime(lifetime), pathCreated(llarp_time_now_ms())
    {
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
    PathConfirmMessage::BDecode(llarp_buffer_t* buf)
    {
      return false;
    }

    bool
    PathConfirmMessage::HandleMessage(llarp_router* r) const
    {
      return true;
    }

  }  // namespace routing
}  // namespace llarp