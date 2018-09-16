#include <llarp/endian.h>
#include <algorithm>
#include <llarp/ip.hpp>
#include "llarp/buffer.hpp"
#include "mem.hpp"
#include <netinet/in.h>
#include <map>

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

    /// first map entry is bytes offset to checksum relative to end of ip header
    /// second map entry is offset from beginning of packet for checksum
    static std::map< byte_t, std::pair< uint16_t, uint16_t > >
        protoChecksumOffsets = {{IPPROTO_TCP, {16, 12}},
                                {IPPROTO_ICMP, {2, 0}},
                                {IPPROTO_UDP, {6, 0}}};

    static uint16_t
    ipchksum(const byte_t *buf, size_t sz)
    {
      uint32_t sum = 0;
      while(sz > 1)
      {
        sum += *(const uint16_t *)buf;
        sz -= sizeof(uint16_t);
        buf += sizeof(uint16_t);
      }
      if(sz > 0)
        sum += *(const byte_t *)buf;

      while(sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

      return ~sum;
    }

    void
    IPv4Packet::UpdateChecksum()
    {
      auto hdr   = Header();
      hdr->check = 0;
      auto len   = hdr->ihl * 4;
      hdr->check = ipchksum(buf, len);

      auto itr = protoChecksumOffsets.find(hdr->protocol);
      if(itr != protoChecksumOffsets.end())
      {
        auto offset     = itr->second.second;
        uint16_t *check = (uint16_t *)(buf + len + itr->second.first);
        *check          = 0;
        *check          = ipchksum(buf + offset, sz - offset);
      }
    }

  }  // namespace net
}  // namespace llarp
