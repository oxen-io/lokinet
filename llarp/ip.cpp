#include <llarp/endian.h>
#include <algorithm>
#include <llarp/ip.hpp>
#include "llarp/buffer.hpp"
#include "mem.hpp"

namespace llarp
{
  namespace net
  {
    bool
    IPv4Packet::Load(llarp_buffer_t pkt)
    {
#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
      sz = MIN(pkt.sz, sizeof(buf));
#undef MIN
#endif
      memcpy(buf, pkt.base, sz);
      return true;
    }

    llarp_buffer_t
    IPv4Packet::Buffer()
    {
      return llarp::InitBuffer(buf, sz);
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
        sum += *(uint16_t *)addr;
        count -= sizeof(uint16_t);
        addr += sizeof(uint16_t);
      }
      if(count > 0)
        sum += *(byte_t *)addr;

      while(sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

      hdr->check = ~sum;
    }

  }  // namespace net
}  // namespace llarp
