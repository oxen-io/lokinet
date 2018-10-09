#include <llarp/endian.h>
#include <algorithm>
#include <llarp/ip.hpp>
#include "llarp/buffer.hpp"
#include "mem.hpp"
#ifndef _WIN32
#include <netinet/in.h>
#endif
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
            /// ICMP
            {1,
             [](const ip_header *hdr, byte_t *buf, size_t sz) {
               auto len        = hdr->ihl * 4;
               uint16_t *check = (uint16_t *)buf + len + 2;
               *check          = 0;
               *check          = ipchksum(buf, sz);
             }},
            /// TCP
            {6, [](const ip_header *hdr, byte_t *pkt, size_t sz) {
               byte_t pktbuf[1500];
               auto len        = hdr->ihl * 4;
               size_t pktsz    = sz - len;
               uint16_t *check = (uint16_t *)(pkt + len + 16);
               *check          = 0;
               memcpy(pktbuf, &hdr->saddr, 4);
               memcpy(pktbuf + 4, &hdr->daddr, 4);
               pktbuf[8] = 0;
               pktbuf[9] = 6;
               // TODO: endian (?)
               pktbuf[10] = (pktsz & 0xff00) >> 8;
               pktbuf[11] = pktsz & 0x00ff;
               memcpy(pktbuf + 12, pkt + len, pktsz);
               *check = ipchksum(pktbuf, 12 + pktsz);
             }}};
    void
    IPv4Packet::UpdateChecksumsOnDst()
    {
      // IPv4 checksum
      auto hdr   = Header();
      hdr->check = 0;
      auto len   = hdr->ihl * 4;
      hdr->check = ipchksum(buf, len);

      // L4 checksum
      auto proto = hdr->protocol;
      auto itr   = protoCheckSummer.find(proto);
      if(itr != protoCheckSummer.end())
      {
        itr->second(hdr, buf, sz);
      }
    }

    void
    IPv4Packet::UpdateChecksumsOnSrc()
    {
      // IPv4
      Header()->check = 0;
    }
  }  // namespace net
}  // namespace llarp
