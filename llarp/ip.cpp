#include <llarp/endian.h>
#include <llarp/ip.hpp>

namespace llarp
{
  namespace net
  {
    std::unique_ptr< IPv4Packet >
    ParseIPv4Packet(const void* buf, size_t sz)
    {
      if(sz < 16 || sz > IPv4Packet::MaxSize)
        return nullptr;
      IPv4Packet* pkt = new IPv4Packet();
      memcpy(pkt->buf, buf, sz);
      return std::unique_ptr< IPv4Packet >(pkt);
    }
  }  // namespace net
}  // namespace llarp
