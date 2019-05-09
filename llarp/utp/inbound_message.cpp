#include <utp/inbound_message.hpp>

#include <string.h>

namespace llarp
{
  namespace utp
  {
    bool
    _InboundMessage::IsExpired(llarp_time_t now) const
    {
      return now > lastActive && now - lastActive >= 2000;
    }

    bool
    _InboundMessage::AppendData(const byte_t* ptr, uint16_t sz)
    {
      if(buffer.size_left() < sz)
        return false;
      memcpy(buffer.cur, ptr, sz);
      buffer.cur += sz;
      return true;
    }
    IBMsgPool_t IBPool;

  }  // namespace utp

}  // namespace llarp
