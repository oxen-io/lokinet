#include <utp/inbound_message.hpp>

#include <cstring>

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
      std::copy_n(ptr, sz, buffer.cur);
      buffer.cur += sz;
      return true;
    }

  }  // namespace utp

}  // namespace llarp
