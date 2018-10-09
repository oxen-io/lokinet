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
      sz = std::min(pkt.sz, sizeof(buf));
      memcpy(buf, pkt.base, sz);
      return true;
    }

    llarp_buffer_t
    IPv4Packet::Buffer()
    {
      return llarp::InitBuffer(buf, sz);
    }

    static uint32_t
    ipchksum_pseudoIPv4(uint32_t src_ip_n, uint32_t dst_ip_n, uint8_t proto,
                        uint16_t innerlen)
    {
#define IPCS(x) ((x & 0xFFFF) + (x >> 16))
      uint32_t sum = (uint32_t)IPCS(src_ip_n) + (uint32_t)IPCS(dst_ip_n)
          + (uint32_t)proto + (uint32_t)htons(innerlen);
#undef IPCS
      return sum;
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

    static uint16_t
    deltachksum(uint16_t old_sum, uint32_t old_src_ip_n, uint32_t old_dst_ip_n,
                uint32_t new_src_ip_n, uint32_t new_dst_ip_n)
    {
#define IPCS(x) ((x & 0xFFFF) + (x >> 16))
      uint32_t sum = ~old_sum + IPCS(new_src_ip_n) + IPCS(new_dst_ip_n)
          - IPCS(old_src_ip_n) - IPCS(old_dst_ip_n);
#undef IPCS
      while(sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
      return ~sum;
    }

    static std::map<
        byte_t, std::function< void(const ip_header *, byte_t *, size_t) > >
        protoDstCheckSummer = {
    // is this even correct???
    // {RFC3022} says that IPv4 hdr isn't included in ICMP checksum calc
    // and that we don't need to modify it
#if 0
      {
        // ICMP
        1,
        [](const ip_header *hdr, byte_t *buf, size_t sz)
        {
          auto len = hdr->ihl * 4;
          if(len + 2 + 2 > sz)
            return;
          uint16_t *check = (uint16_t *)(buf + len + 2);

          *check = 0;
          *check = ipchksum(buf, sz);
        }
      },
#endif
            {// TCP
             6,
             [](const ip_header *hdr, byte_t *pkt, size_t sz) {
               auto hlen = size_t(hdr->ihl * 4);
               uint16_t *check = (uint16_t *)(pkt + hlen + 16);
               *check = deltachksum(*check, 0, 0, hdr->saddr, hdr->daddr);
             }},
    };
    void
    IPv4Packet::UpdateChecksumsOnDst()
    {
      auto hdr = Header();

      // IPv4 checksum
      auto hlen  = size_t(hdr->ihl * 4);
      hdr->check = 0;
      hdr->check = ipchksum(buf, hlen);

      // L4 checksum
      auto proto = hdr->protocol;
      auto itr   = protoDstCheckSummer.find(proto);
      if(itr != protoDstCheckSummer.end())
      {
        itr->second(hdr, buf, sz);
      }
    }

    static std::map<
        byte_t, std::function< void(const ip_header *, byte_t *, size_t) > >
        protoSrcCheckSummer = {
            {// TCP
             6,
             [](const ip_header *hdr, byte_t *pkt, size_t sz) {
               auto hlen = size_t(hdr->ihl * 4);
               uint16_t *check = (uint16_t *)(pkt + hlen + 16);
               *check = deltachksum(*check, hdr->saddr, hdr->daddr, 0, 0);
             }},
    };
    void
    IPv4Packet::UpdateChecksumsOnSrc()
    {
      auto hdr = Header();

      // L4
      auto proto = hdr->protocol;
      auto itr   = protoSrcCheckSummer.find(proto);
      if(itr != protoSrcCheckSummer.end())
      {
        itr->second(hdr, buf, sz);
      }

      // IPv4
      hdr->check = 0;
    }
  }  // namespace net
}  // namespace llarp
