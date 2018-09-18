#include <llarp/endian.h>
#include <algorithm>
#include <llarp/ip.hpp>
#include "llarp/buffer.hpp"
#include "mem.hpp"
#include <netinet/in.h>
#include <llarp/endian.h>
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

    static uint16_t
    ipchksum(const byte_t *buf, size_t sz, uint32_t sum = 0)
    {
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

    static std::map<
        byte_t, std::function< void(const ip_header *, byte_t *, size_t) > >
        protoCheckSummer = {
            {IPPROTO_ICMP,
             [](const ip_header *hdr, byte_t *buf, size_t sz) {
               auto len        = hdr->ihl * 4;
               uint16_t *check = (uint16_t *)buf + len + 2;
               *check          = 0;
               *check          = ipchksum(buf, sz);
             }},
            {IPPROTO_TCP, [](const ip_header *hdr, byte_t *pkt, size_t sz) {
               auto len        = hdr->ihl * 4;
               uint16_t *check = (uint16_t *)pkt + 28 + len;
               *check          = 0;
               *check =
                   ipchksum(pkt, sz - len,
                            hdr->saddr + hdr->daddr + IPPROTO_TCP + (sz - len));
             }}};
    void
    IPv4Packet::UpdateChecksum()
    {
      auto hdr   = Header();
      hdr->check = 0;
      auto len   = hdr->ihl * 4;
      hdr->check = ipchksum(buf, len);
      auto proto = hdr->protocol;
      auto itr   = protoCheckSummer.find(proto);
      if(itr != protoCheckSummer.end())
      {
        itr->second(hdr, buf, sz);
      }
    }
  }  // namespace net
}  // namespace llarp
