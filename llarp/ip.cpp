#include <llarp/endian.hpp>
#include <algorithm>
#include <llarp/ip.hpp>
#include "llarp/buffer.hpp"
#include "mem.hpp"
#ifndef _WIN32
#include <netinet/in.h>
#endif
#include <map>
#include <algorithm>

namespace llarp
{
  namespace net
  {
    bool
    IPv4Packet::Load(llarp_buffer_t pkt)
    {
      if(pkt.sz > sizeof(buf))
        return false;
      sz = pkt.sz;
      memcpy(buf, pkt.base, sz);
      return true;
    }

    llarp_buffer_t
    IPv4Packet::ConstBuffer() const
    {
      return llarp::InitBuffer(buf, sz);
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
        uint16_t x = 0;

        *(byte_t *)&x = *(const byte_t *)buf;
        sum += x;
      }

      // only need to do it 2 times to be sure
      // proof: 0xFFff + 0xFFff = 0x1FFfe -> 0xFFff
      sum = (sum & 0xFFff) + (sum >> 16);
      sum += sum >> 16;

      return uint16_t((~sum) & 0xFFff);
    }
#endif

    static nuint16_t
    deltaIPv4Checksum(nuint16_t old_sum, nuint32_t old_src_ip,
                      nuint32_t old_dst_ip, nuint32_t new_src_ip,
                      nuint32_t new_dst_ip)
    {
#define ADDIPCS(x) ((uint32_t)(x.n & 0xFFff) + (uint32_t)(x.n >> 16))
#define SUBIPCS(x) ((uint32_t)((~x.n) & 0xFFff) + (uint32_t)((~x.n) >> 16))

      uint32_t sum = uint32_t(old_sum.n) + ADDIPCS(old_src_ip)
          + ADDIPCS(old_dst_ip) + SUBIPCS(new_src_ip) + SUBIPCS(new_dst_ip);

#undef ADDIPCS
#undef SUBIPCS

      // only need to do it 2 times to be sure
      // proof: 0xFFff + 0xFFff = 0x1FFfe -> 0xFFff
      sum = (sum & 0xFFff) + (sum >> 16);
      sum += sum >> 16;

      return nuint16_t{uint16_t(sum & 0xFFff)};
    }

    static void
    checksumDstIPv4TCP(byte_t *pld, __attribute__((unused)) size_t psz,
                       size_t fragoff, size_t chksumoff, nuint32_t oSrcIP,
                       nuint32_t oDstIP, nuint32_t nSrcIP, nuint32_t nDstIP)
    {
      if(fragoff > chksumoff)
        return;

      auto check = (nuint16_t *)(pld + chksumoff - fragoff);

      *check = deltaIPv4Checksum(*check, oSrcIP, oDstIP, nSrcIP, nDstIP);
      // usually, TCP checksum field cannot be 0xFFff,
      // because one's complement addition cannot result in 0x0000,
      // and there's inversion in the end;
      // emulate that.
      if(check->n == 0xFFff)
        check->n = 0x0000;
    }

    static void
    checksumDstIPv4UDP(byte_t *pld, __attribute__((unused)) size_t psz,
                       size_t fragoff, nuint32_t oSrcIP, nuint32_t oDstIP,
                       nuint32_t nSrcIP, nuint32_t nDstIP)
    {
      if(fragoff > 6)
        return;

      auto check = (nuint16_t *)(pld + 6);
      if(check->n == 0x0000)
        return;  // 0 is used to indicate "no checksum", don't change

      *check = deltaIPv4Checksum(*check, oSrcIP, oDstIP, nSrcIP, nDstIP);
      // 0 is used to indicate "no checksum"
      // 0xFFff and 0 are equivalent in one's complement math
      // 0xFFff + 1 = 0x10000 -> 0x0001 (same as 0 + 1)
      // infact it's impossible to get 0 with such addition,
      // when starting from non-0 value.
      // inside deltachksum we don't invert so it's safe to skip check there
      // if(check->n == 0x0000)
      //   check->n = 0xFFff;
    }

    void
    IPv4Packet::UpdateIPv4PacketOnDst(huint32_t newSrcIP, huint32_t newDstIP)
    {
      auto hdr = Header();

      auto oSrcIP = nuint32_t{hdr->saddr};
      auto oDstIP = nuint32_t{hdr->daddr};
      auto nSrcIP = xhtonl(newSrcIP);
      auto nDstIP = xhtonl(newDstIP);

      // IPv4 checksum
      auto v4chk = (nuint16_t *)&(hdr->check);
      *v4chk     = deltaIPv4Checksum(*v4chk, oSrcIP, oDstIP, nSrcIP, nDstIP);

      // L4 checksum
      auto ihs = size_t(hdr->ihl * 4);
      if(ihs <= sz)
      {
        auto pld = buf + ihs;
        auto psz = sz - ihs;

        auto fragoff = size_t((ntohs(hdr->frag_off) & 0x1Fff) * 8);

        switch(hdr->protocol)
        {
          case 6:  // TCP
            checksumDstIPv4TCP(pld, psz, fragoff, 16, oSrcIP, oDstIP, nSrcIP,
                               nDstIP);
            break;
          case 17:   // UDP
          case 136:  // UDP-Lite - same checksum place, same 0->0xFFff condition
            checksumDstIPv4UDP(pld, psz, fragoff, oSrcIP, oDstIP, nSrcIP,
                               nDstIP);
            break;
          case 33:  // DCCP
            checksumDstIPv4TCP(pld, psz, fragoff, 6, oSrcIP, oDstIP, nSrcIP,
                               nDstIP);
            break;
        }
      }

      // write new IP addresses
      hdr->saddr = nSrcIP.n;
      hdr->daddr = nDstIP.n;
    }

    static void
    checksumSrcIPv4TCP(byte_t *pld, __attribute__((unused)) size_t psz,
                       size_t fragoff, size_t chksumoff, nuint32_t oSrcIP,
                       nuint32_t oDstIP)
    {
      if(fragoff > chksumoff)
        return;

      auto check = (nuint16_t *)(pld + chksumoff - fragoff);

      *check =
          deltaIPv4Checksum(*check, oSrcIP, oDstIP, nuint32_t{0}, nuint32_t{0});
      // usually, TCP checksum field cannot be 0xFFff,
      // because one's complement addition cannot result in 0x0000,
      // and there's inversion in the end;
      // emulate that.
      if(check->n == 0xFFff)
        check->n = 0x0000;
    }

    static void
    checksumSrcIPv4UDP(byte_t *pld, __attribute__((unused)) size_t psz,
                       size_t fragoff, nuint32_t oSrcIP, nuint32_t oDstIP)
    {
      if(fragoff > 6)
        return;

      auto check = (nuint16_t *)(pld + 6);
      if(check->n == 0x0000)
        return;  // 0 is used to indicate "no checksum", don't change

      *check =
          deltaIPv4Checksum(*check, oSrcIP, oDstIP, nuint32_t{0}, nuint32_t{0});
      // 0 is used to indicate "no checksum"
      // 0xFFff and 0 are equivalent in one's complement math
      // 0xFFff + 1 = 0x10000 -> 0x0001 (same as 0 + 1)
      // infact it's impossible to get 0 with such addition,
      // when starting from non-0 value.
      // inside deltachksum we don't invert so it's safe to skip check there
      // if(check->n == 0x0000)
      //   check->n = 0xFFff;
    }

    void
    IPv4Packet::UpdateIPv4PacketOnSrc()
    {
      auto hdr = Header();

      auto oSrcIP = nuint32_t{hdr->saddr};
      auto oDstIP = nuint32_t{hdr->daddr};

      // L4
      auto ihs = size_t(hdr->ihl * 4);
      if(ihs <= sz)
      {
        auto pld = buf + ihs;
        auto psz = sz - ihs;

        auto fragoff = size_t((ntohs(hdr->frag_off) & 0x1Fff) * 8);

        switch(hdr->protocol)
        {
          case 6:  // TCP
            checksumSrcIPv4TCP(pld, psz, fragoff, 16, oSrcIP, oDstIP);
            break;
          case 17:   // UDP
          case 136:  // UDP-Lite
            checksumSrcIPv4UDP(pld, psz, fragoff, oSrcIP, oDstIP);
            break;
          case 33:  // DCCP
            checksumSrcIPv4TCP(pld, psz, fragoff, 6, oSrcIP, oDstIP);
            break;
        }
      }

      // IPv4
      auto v4chk = (nuint16_t *)&(hdr->check);
      *v4chk =
          deltaIPv4Checksum(*v4chk, oSrcIP, oDstIP, nuint32_t{0}, nuint32_t{0});

      // clear addresses
      hdr->saddr = 0;
      hdr->daddr = 0;
    }
  }  // namespace net
}  // namespace llarp
