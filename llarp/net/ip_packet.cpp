#include <net/ip_packet.hpp>
#include <net/ip.hpp>

#include <util/buffer.hpp>
#include <util/endian.hpp>
#include <util/mem.hpp>

#ifndef _WIN32
#include <netinet/in.h>
#endif

#include <algorithm>
#include <map>

namespace llarp
{
  namespace net
  {
    inline static uint32_t*
    in6_uint32_ptr(in6_addr& addr)
    {
      return (uint32_t*)addr.s6_addr;
    }

    inline static const uint32_t*
    in6_uint32_ptr(const in6_addr& addr)
    {
      return (uint32_t*)addr.s6_addr;
    }

    huint128_t
    IPPacket::srcv6() const
    {
      if (IsV6())
        return In6ToHUInt(HeaderV6()->srcaddr);

      return ExpandV4(srcv4());
    }

    huint128_t
    IPPacket::dstv6() const
    {
      if (IsV6())
        return In6ToHUInt(HeaderV6()->dstaddr);

      return ExpandV4(dstv4());
    }

    bool
    IPPacket::Load(const llarp_buffer_t& pkt)
    {
      if (pkt.sz > sizeof(buf) or pkt.sz == 0)
        return false;
      sz = pkt.sz;
      std::copy_n(pkt.base, sz, buf);
      return true;
    }

    ManagedBuffer
    IPPacket::ConstBuffer() const
    {
      const byte_t* ptr = buf;
      llarp_buffer_t b(ptr, sz);
      return ManagedBuffer(b);
    }

    ManagedBuffer
    IPPacket::Buffer()
    {
      byte_t* ptr = buf;
      llarp_buffer_t b(ptr, sz);
      return ManagedBuffer(b);
    }

    huint32_t
    IPPacket::srcv4() const
    {
      return huint32_t{ntohl(Header()->saddr)};
    }

    huint32_t
    IPPacket::dstv4() const
    {
      return huint32_t{ntohl(Header()->daddr)};
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

#define ADD32CS(x) ((uint32_t)(x & 0xFFff) + (uint32_t)(x >> 16))
#define SUB32CS(x) ((uint32_t)((~x) & 0xFFff) + (uint32_t)((~x) >> 16))

    static nuint16_t
    deltaIPv4Checksum(
        nuint16_t old_sum,
        nuint32_t old_src_ip,
        nuint32_t old_dst_ip,
        nuint32_t new_src_ip,
        nuint32_t new_dst_ip)
    {
      uint32_t sum = uint32_t(old_sum.n) + ADD32CS(old_src_ip.n) + ADD32CS(old_dst_ip.n)
          + SUB32CS(new_src_ip.n) + SUB32CS(new_dst_ip.n);

      // only need to do it 2 times to be sure
      // proof: 0xFFff + 0xFFff = 0x1FFfe -> 0xFFff
      sum = (sum & 0xFFff) + (sum >> 16);
      sum += sum >> 16;

      return nuint16_t{uint16_t(sum & 0xFFff)};
    }

    static nuint16_t
    deltaIPv6Checksum(
        nuint16_t old_sum,
        const uint32_t old_src_ip[4],
        const uint32_t old_dst_ip[4],
        const uint32_t new_src_ip[4],
        const uint32_t new_dst_ip[4])
    {
      /* we don't actually care in what way integers are arranged in memory
       * internally */
      /* as long as uint16 pairs are swapped in correct direction, result will
       * be correct (assuming there are no gaps in structure) */
      /* we represent 128bit stuff there as 4 32bit ints, that should be more or
       * less correct */
      /* we could do 64bit ints too but then we couldn't reuse 32bit macros and
       * that'd suck for 32bit cpus */
#define ADDN128CS(x) (ADD32CS(x[0]) + ADD32CS(x[1]) + ADD32CS(x[2]) + ADD32CS(x[3]))
#define SUBN128CS(x) (SUB32CS(x[0]) + SUB32CS(x[1]) + SUB32CS(x[2]) + SUB32CS(x[3]))
      uint32_t sum = uint32_t(old_sum.n) + ADDN128CS(old_src_ip) + ADDN128CS(old_dst_ip)
          + SUBN128CS(new_src_ip) + SUBN128CS(new_dst_ip);
#undef ADDN128CS
#undef SUBN128CS

      // only need to do it 2 times to be sure
      // proof: 0xFFff + 0xFFff = 0x1FFfe -> 0xFFff
      sum = (sum & 0xFFff) + (sum >> 16);
      sum += sum >> 16;

      return nuint16_t{uint16_t(sum & 0xFFff)};
    }

#undef ADD32CS
#undef SUB32CS

    static void
    deltaChecksumIPv4TCP(
        byte_t* pld,
        size_t psz,
        size_t fragoff,
        size_t chksumoff,
        nuint32_t oSrcIP,
        nuint32_t oDstIP,
        nuint32_t nSrcIP,
        nuint32_t nDstIP)
    {
      if (fragoff > chksumoff || psz < chksumoff - fragoff + 2)
        return;

      auto check = (nuint16_t*)(pld + chksumoff - fragoff);

      *check = deltaIPv4Checksum(*check, oSrcIP, oDstIP, nSrcIP, nDstIP);
      // usually, TCP checksum field cannot be 0xFFff,
      // because one's complement addition cannot result in 0x0000,
      // and there's inversion in the end;
      // emulate that.
      if (check->n == 0xFFff)
        check->n = 0x0000;
    }

    static void
    deltaChecksumIPv6TCP(
        byte_t* pld,
        size_t psz,
        size_t fragoff,
        size_t chksumoff,
        const uint32_t oSrcIP[4],
        const uint32_t oDstIP[4],
        const uint32_t nSrcIP[4],
        const uint32_t nDstIP[4])
    {
      if (fragoff > chksumoff || psz < chksumoff - fragoff + 2)
        return;

      auto check = (nuint16_t*)(pld + chksumoff - fragoff);

      *check = deltaIPv6Checksum(*check, oSrcIP, oDstIP, nSrcIP, nDstIP);
      // usually, TCP checksum field cannot be 0xFFff,
      // because one's complement addition cannot result in 0x0000,
      // and there's inversion in the end;
      // emulate that.
      if (check->n == 0xFFff)
        check->n = 0x0000;
    }

    static void
    deltaChecksumIPv4UDP(
        byte_t* pld,
        size_t psz,
        size_t fragoff,
        nuint32_t oSrcIP,
        nuint32_t oDstIP,
        nuint32_t nSrcIP,
        nuint32_t nDstIP)
    {
      if (fragoff > 6 || psz < 6 + 2)
        return;

      auto check = (nuint16_t*)(pld + 6);
      if (check->n == 0x0000)
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

    static void
    deltaChecksumIPv6UDP(
        byte_t* pld,
        size_t psz,
        size_t fragoff,
        const uint32_t oSrcIP[4],
        const uint32_t oDstIP[4],
        const uint32_t nSrcIP[4],
        const uint32_t nDstIP[4])
    {
      if (fragoff > 6 || psz < 6 + 2)
        return;

      auto check = (nuint16_t*)(pld + 6);
      // 0 is used to indicate "no checksum", don't change
      // even tho this shouldn't happen for IPv6, handle it properly
      // we actually should drop/log 0-checksum packets per spec
      // but that should be done at upper level than this function
      // it's better to do correct thing there regardless
      // XXX or maybe we should change this function to be able to return error?
      // either way that's not a priority
      if (check->n == 0x0000)
        return;

      *check = deltaIPv6Checksum(*check, oSrcIP, oDstIP, nSrcIP, nDstIP);
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
    IPPacket::UpdateIPv4Address(nuint32_t nSrcIP, nuint32_t nDstIP)
    {
      llarp::LogDebug("set src=", nSrcIP, " dst=", nDstIP);

      auto hdr = Header();

      auto oSrcIP = nuint32_t{hdr->saddr};
      auto oDstIP = nuint32_t{hdr->daddr};

      // L4 checksum
      auto ihs = size_t(hdr->ihl * 4);
      if (ihs <= sz)
      {
        auto pld = buf + ihs;
        auto psz = sz - ihs;

        auto fragoff = size_t((ntohs(hdr->frag_off) & 0x1Fff) * 8);

        switch (hdr->protocol)
        {
          case 6:  // TCP
            deltaChecksumIPv4TCP(pld, psz, fragoff, 16, oSrcIP, oDstIP, nSrcIP, nDstIP);
            break;
          case 17:   // UDP
          case 136:  // UDP-Lite - same checksum place, same 0->0xFFff condition
            deltaChecksumIPv4UDP(pld, psz, fragoff, oSrcIP, oDstIP, nSrcIP, nDstIP);
            break;
          case 33:  // DCCP
            deltaChecksumIPv4TCP(pld, psz, fragoff, 6, oSrcIP, oDstIP, nSrcIP, nDstIP);
            break;
        }
      }

      // IPv4 checksum
      auto v4chk = (nuint16_t*)&(hdr->check);
      *v4chk = deltaIPv4Checksum(*v4chk, oSrcIP, oDstIP, nSrcIP, nDstIP);

      // write new IP addresses
      hdr->saddr = nSrcIP.n;
      hdr->daddr = nDstIP.n;
    }

    void
    IPPacket::UpdateIPv6Address(huint128_t src, huint128_t dst)
    {
      const size_t ihs = 4 + 4 + 16 + 16;

      // XXX should've been checked at upper level?
      if (sz <= ihs)
        return;

      auto hdr = HeaderV6();

      const auto oldSrcIP = hdr->srcaddr;
      const auto oldDstIP = hdr->dstaddr;
      const uint32_t* oSrcIP = in6_uint32_ptr(oldSrcIP);
      const uint32_t* oDstIP = in6_uint32_ptr(oldDstIP);

      // IPv6 address
      hdr->srcaddr = HUIntToIn6(src);
      hdr->dstaddr = HUIntToIn6(dst);
      const uint32_t* nSrcIP = in6_uint32_ptr(hdr->srcaddr);
      const uint32_t* nDstIP = in6_uint32_ptr(hdr->dstaddr);

      // TODO IPv6 header options
      auto pld = buf + ihs;
      auto psz = sz - ihs;

      size_t fragoff = 0;
      auto nextproto = hdr->proto;
      for (;;)
      {
        switch (nextproto)
        {
          case 0:   // Hop-by-Hop Options
          case 43:  // Routing Header
          case 60:  // Destination Options
          {
            nextproto = pld[0];
            auto addlen = (size_t(pld[1]) + 1) * 8;
            if (psz < addlen)
              return;
            pld += addlen;
            psz -= addlen;
            break;
          }

          case 44:  // Fragment Header
            /*
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Next Header  |   Reserved    |      Fragment Offset    |Res|M|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                         Identification                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             */
            nextproto = pld[0];
            fragoff = (uint16_t(pld[2]) << 8) | (uint16_t(pld[3]) & 0xFC);
            if (psz < 8)
              return;
            pld += 8;
            psz -= 8;

            // jump straight to payload processing
            if (fragoff != 0)
              goto endprotohdrs;
            break;

          default:
            goto endprotohdrs;
        }
      }
    endprotohdrs:

      switch (nextproto)
      {
        case 6:  // TCP
          deltaChecksumIPv6TCP(pld, psz, fragoff, 16, oSrcIP, oDstIP, nSrcIP, nDstIP);
          break;
        case 17:   // UDP
        case 136:  // UDP-Lite - same checksum place, same 0->0xFFff condition
          deltaChecksumIPv6UDP(pld, psz, fragoff, oSrcIP, oDstIP, nSrcIP, nDstIP);
          break;
        case 33:  // DCCP
          deltaChecksumIPv6TCP(pld, psz, fragoff, 6, oSrcIP, oDstIP, nSrcIP, nDstIP);
          break;
      }
    }
  }  // namespace net
}  // namespace llarp
