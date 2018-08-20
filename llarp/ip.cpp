#include <llarp/endian.h>
#include <llarp/ip.hpp>

namespace llarp
{
  namespace net
  {
    bool
    IPv4Packet::Load(llarp_buffer_t pkt)
    {
      memcpy(buf, pkt.base, std::min(pkt.sz, sizeof(buf)));
      return true;
    }

    void
    IPv4Packet::UpdateChecksum()
    {
      auto hdr   = Header();
      hdr->check = 0;

      size_t count = hdr->ihl;
      uint32_t sum = 0;
      byte_t *addr = buf;

      while(count > 1)
      {
        sum += ntohs(*(uint16_t *)addr);
        count -= sizeof(uint16_t);
        addr += sizeof(uint16_t);
      }
      if(count > 0)
        sum += *(byte_t *)addr;

      while(sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

      hdr->check = htons(~sum);
    }

  }  // namespace net
}  // namespace llarp
