#include <link/utp_inbound_message.hpp>

#include <string.h>

namespace llarp
{
  namespace utp
  {
    bool
    InboundMessage::IsExpired(llarp_time_t now) const
    {
      return now > lastActive && now - lastActive >= 2000;
    }

    bool
    InboundMessage::AppendData(const byte_t* ptr, uint16_t sz)
    {
      if(buffer.size_left() < sz)
        return false;
      memcpy(buffer.cur, ptr, sz);
      buffer.cur += sz;
      return true;
    }
  }  // namespace utp

}  // namespace llarp
