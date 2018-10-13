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
#include <algorithm>

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

#if 0
    static uint32_t
    ipchksum_pseudoIPv4(nuint32_t src_ip, nuint32_t dst_ip, uint8_t proto,
                        uint16_t innerlen)
    {
#define IPCS(x) ((uint32_t)(x & 0xFFff) + (uint32_t)(x >> 16))
      uint32_t sum = IPCS(src_ip.n) + IPCS(dst_ip.n) + (uint32_t)proto
          + (uint32_t)htons(innerlen);
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
      if(sz != 0)
      {
        uint16_t x    = 0;
        *(byte_t *)&x = *(const byte_t *)buf;
        sum += x;
      }

      while(sum >> 16)
        sum = (sum & 0xFFff) + (sum >> 16);

      return ~sum;
    }
#endif

    static uint16_t
    deltachksum(uint16_t old_sum, huint32_t old_src_ip, huint32_t old_dst_ip,
                huint32_t new_src_ip, huint32_t new_dst_ip)
    {
#define ADDIPCS(x) ((uint32_t)(x.h & 0xFFff) + (uint32_t)(x.h >> 16))
#define SUBIPCS(x) ((uint32_t)((~x.h) & 0xFFff) + (uint32_t)((~x.h) >> 16))

      uint32_t sum = ntohs(old_sum) + ADDIPCS(old_src_ip) + ADDIPCS(old_dst_ip)
          + SUBIPCS(new_src_ip) + SUBIPCS(new_dst_ip);

#undef ADDIPCS
#undef SUBIPCS

      while(sum >> 16)
        sum = (sum & 0xFFff) + (sum >> 16);

      return htons(sum);
    }

    static void
    checksumDstTCP(byte_t *pld, size_t psz, size_t fragoff, huint32_t oSrcIP,
                   huint32_t oDstIP, huint32_t nSrcIP, huint32_t nDstIP)
    {
      if(fragoff > 16)
        return;

      uint16_t *check = (uint16_t *)(pld + 16 - fragoff);

      *check = deltachksum(*check, oSrcIP, oDstIP, nSrcIP, nDstIP);
    }

    static void
    checksumDstUDP(const ip_header *ohdr, byte_t *pld, size_t psz,
                   size_t fragoff, huint32_t oSrcIP, huint32_t oDstIP,
                   huint32_t nSrcIP, huint32_t nDstIP)
    {
      if(fragoff > 6)
        return;

      uint16_t *check = (uint16_t *)(pld + 6);
      if(*check == 0x0000)
        return;  // 0 is used to indicate "no checksum", don't change

      *check = deltachksum(*check, oSrcIP, oDstIP, nSrcIP, nDstIP);

      // 0 is used to indicate "no checksum"
      // 0xFFff and 0 are equivalent in one's complement math
      // 0xFFff + 1 = 0x10000 -> 0x0001 (same as 0 + 1)
      // infact it's impossible to get 0 with such addition
      // when starting from non-0 value
      // but it's possible to get 0xFFff and we invert after that
      // so we still need this fixup check
      if(*check == 0x0000)
        *check = 0xFFff;
    }

    void
    IPv4Packet::UpdatePacketOnDst(huint32_t nSrcIP, huint32_t nDstIP)
    {
      auto hdr = Header();

      auto oSrcIP = xntohl(nuint32_t{hdr->saddr});
      auto oDstIP = xntohl(nuint32_t{hdr->daddr});

      // IPv4 checksum
      hdr->check = deltachksum(hdr->check, oSrcIP, oDstIP, nSrcIP, nDstIP);

      // L4 checksum
      auto ihs = size_t(hdr->ihl * 4);
      if(ihs <= sz)
      {
        auto pld = buf + ihs;
        auto psz = sz - ihs;

        auto fragoff = size_t((ntohs(hdr->frag_off) & 0x1Fff) * 8);

        switch(hdr->protocol)
        {
          case 6:
            checksumDstTCP(pld, psz, fragoff, oSrcIP, oDstIP, nSrcIP, nDstIP);
            break;
          case 17:
            checksumDstUDP(hdr, pld, psz, fragoff, oSrcIP, oDstIP, nSrcIP,
                           nDstIP);
            break;
        }
      }

      // write new IP addresses
      hdr->saddr = xhtonl(nSrcIP).n;
      hdr->daddr = xhtonl(nDstIP).n;
    }

    static void
    checksumSrcTCP(byte_t *pld, size_t psz, size_t fragoff, huint32_t oSrcIP,
                   huint32_t oDstIP)
    {
      if(fragoff > 16)
        return;

      uint16_t *check = (uint16_t *)(pld + 16 - fragoff);

      *check = deltachksum(*check, oSrcIP, oDstIP, huint32_t{0}, huint32_t{0});
    }

    static void
    checksumSrcUDP(const ip_header *ohdr, byte_t *pld, size_t psz,
                   size_t fragoff, huint32_t oSrcIP, huint32_t oDstIP)
    {
      if(fragoff > 6)
        return;

      uint16_t *check = (uint16_t *)(pld + 6);
      if(*check == 0x0000)
        return;  // 0 is used to indicate "no checksum", don't change

      *check = deltachksum(*check, oSrcIP, oDstIP, huint32_t{0}, huint32_t{0});

      // 0 is used to indicate "no checksum"
      // 0xFFff and 0 are equivalent in one's complement math
      // 0xFFff + 1 = 0x10000 -> 0x0001 (same as 0 + 1)
      // infact it's impossible to get 0 with such addition
      // when starting from non-0 value
      // but it's possible to get 0xFFff and we invert after that
      // so we still need this fixup check
      if(*check == 0x0000)
        *check = 0xFFff;
    }

    void
    IPv4Packet::UpdatePacketOnSrc()
    {
      auto hdr = Header();

      auto oSrcIP = xntohl(nuint32_t{hdr->saddr});
      auto oDstIP = xntohl(nuint32_t{hdr->daddr});

      // L4
      auto ihs = size_t(hdr->ihl * 4);
      if(ihs <= sz)
      {
        auto pld = buf + ihs;
        auto psz = sz - ihs;

        auto fragoff = size_t((ntohs(hdr->frag_off) & 0x1Fff) * 8);

        switch(hdr->protocol)
        {
          case 6:
            checksumSrcTCP(pld, psz, fragoff, oSrcIP, oDstIP);
            break;
          case 17:
            checksumSrcUDP(hdr, pld, psz, fragoff, oSrcIP, oDstIP);
            break;
        }
      }

      // IPv4
      hdr->check =
          deltachksum(hdr->check, oSrcIP, oDstIP, huint32_t{0}, huint32_t{0});

      // clear addresses
      hdr->saddr = 0;
      hdr->daddr = 0;
    }
  }  // namespace net
}  // namespace llarp
