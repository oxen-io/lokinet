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

    static uint32_t
    ipchksum_pseudoIPv4(nuint32_t src_ip, nuint32_t dst_ip, uint8_t proto,
                        uint16_t innerlen)
    {
#define IPCS(x) ((uint32_t)(x & 0xFFFF) + (uint32_t)(x >> 16))
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
      if(sz > 0)
        sum += *(const byte_t *)buf;

      while(sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

      return ~sum;
    }

    static uint16_t
    deltachksum(uint16_t old_sum, huint32_t old_src_ip, huint32_t old_dst_ip,
                huint32_t new_src_ip, huint32_t new_dst_ip)
    {
#define ADDIPCS(x) ((uint32_t)(x.h & 0xFFFF) + (uint32_t)(x.h >> 16))
#define SUBIPCS(x) ((uint32_t)((~x.h) & 0xFFFF) + (uint32_t)((~x.h) >> 16))

      uint32_t sum = ntohs(old_sum) + ADDIPCS(old_src_ip) + ADDIPCS(old_dst_ip)
          + SUBIPCS(new_src_ip) + SUBIPCS(new_dst_ip);

#undef ADDIPCS
#undef SUBIPCS

      while(sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

      return htons(sum);
    }

    static std::map<
        byte_t,
        std::function< void(const ip_header *ohdr, byte_t *pld, size_t psz,
                            huint32_t oSrcIP, huint32_t oDstIP,
                            huint32_t nSrcIP, huint32_t nDstIP) > >
        protoDstCheckSummer = {
            // {RFC3022} says that IPv4 hdr isn't included in ICMP checksum calc
            // and that we don't need to modify it
            {// TCP
             6,
             [](const ip_header *ohdr, byte_t *pld, size_t psz,
                huint32_t oSrcIP, huint32_t oDstIP, huint32_t nSrcIP,
                huint32_t nDstIP) {
               uint16_t *check = (uint16_t *)(pld + 16);

               *check = deltachksum(*check, oSrcIP, oDstIP, nSrcIP, nDstIP);
             }},
            {// UDP
             17,
             [](const ip_header *ohdr, byte_t *pld, size_t psz,
                huint32_t oSrcIP, huint32_t oDstIP, huint32_t nSrcIP,
                huint32_t nDstIP) {
               uint16_t *check = (uint16_t *)(pld + 6);
               if(*check != 0xFFff)
               {
                 if(*check == 0x0000)
                   return;  // don't change zero

                 *check = deltachksum(*check, oSrcIP, oDstIP, nSrcIP, nDstIP);
                 if(*check == 0x0000)
                   *check = 0xFFff;
               }
               else
               {
                 // such checksum can mean 2 things: 0x0000 or 0xFFff
                 // we can only know by looking at data :<

                 auto pakcs = *check;  // save

                 *check = 0;  // zero checksum before calculation

                 auto cs = ipchksum(
                     pld, psz,
                     ipchksum_pseudoIPv4(nuint32_t{ohdr->saddr},
                                         nuint32_t{ohdr->daddr}, 17, psz));

                 auto new_cs = deltachksum(cs, oSrcIP, oDstIP, nSrcIP, nDstIP);

                 if(cs != 0x0000 && cs != 0xFFff)
                 {
                   // packet was bad - sabotage new checksum
                   new_cs += pakcs - cs;
                 }
                 // 0x0000 is reserved for no checksum
                 if(new_cs == 0x0000)
                   new_cs = 0xFFff;
                 // put it in
                 *check = new_cs;
               }
             }},

    };
    void
    IPv4Packet::UpdatePacketOnDst(huint32_t nSrcIP, huint32_t nDstIP)
    {
      auto hdr = Header();

      auto oSrcIP = xntohl(nuint32_t{hdr->saddr});
      auto oDstIP = xntohl(nuint32_t{hdr->daddr});

      // IPv4 checksum
      hdr->check = deltachksum(hdr->check, oSrcIP, oDstIP, nSrcIP, nDstIP);

      // L4 checksum
      auto proto = hdr->protocol;
      auto itr   = protoDstCheckSummer.find(proto);
      size_t ihs;
      if(itr != protoDstCheckSummer.end() && (ihs = size_t(hdr->ihl * 4)) <= sz)
      {
        itr->second(hdr, buf + ihs, sz - ihs, oSrcIP, oDstIP, nSrcIP, nDstIP);
      }

      // write new IP addresses
      hdr->saddr = xhtonl(nSrcIP).n;
      hdr->daddr = xhtonl(nDstIP).n;
    }

    static std::map<
        byte_t,
        std::function< void(const ip_header *ohdr, byte_t *pld, size_t psz,
                            huint32_t oSrcIP, huint32_t oDstIP) > >
        protoSrcCheckSummer = {
            {// TCP
             6,
             [](const ip_header *ohdr, byte_t *pld, size_t psz,
                huint32_t oSrcIP, huint32_t oDstIP) {
               uint16_t *check = (uint16_t *)(pld + 16);

               *check = deltachksum(*check, oSrcIP, oDstIP, huint32_t{0},
                                    huint32_t{0});
             }},
            {// UDP
             17,
             [](const ip_header *ohdr, byte_t *pld, size_t psz,
                huint32_t oSrcIP, huint32_t oDstIP) {
               uint16_t *check = (uint16_t *)(pld + 6);
               if(*check != 0xFFff)
               {
                 if(*check == 0x0000)
                   return;  // don't change zero

                 *check = deltachksum(*check, oSrcIP, oDstIP, huint32_t{0},
                                      huint32_t{0});
                 if(*check == 0x0000)
                   *check = 0xFFff;
               }
               else
               {
                 // such checksum can mean 2 things: 0x0000 or 0xFFff
                 // we can only know by looking at data :<

                 auto pakcs = *check; // save

                 *check = 0;  // zero checksum before calculation

                 auto cs = ipchksum(
                     pld, psz,
                     ipchksum_pseudoIPv4(nuint32_t{ohdr->saddr},
                                         nuint32_t{ohdr->daddr}, 17, psz));

                 auto new_cs = deltachksum(cs, oSrcIP, oDstIP, huint32_t{0},
                                           huint32_t{0});

                 if(cs != 0x0000 && cs != 0xFFff)
                 {
                   // packet was bad - sabotage new checksum
                   new_cs += pakcs - cs;
                 }
                 // 0x0000 is reserved for no checksum
                 if(new_cs == 0x0000)
                   new_cs = 0xFFff;
                 // put it in
                 *check = new_cs;
               }
             }},
    };
    void
    IPv4Packet::UpdatePacketOnSrc()
    {
      auto hdr = Header();

      auto oSrcIP = xntohl(nuint32_t{hdr->saddr});
      auto oDstIP = xntohl(nuint32_t{hdr->daddr});

      // L4
      auto proto = hdr->protocol;
      auto itr   = protoSrcCheckSummer.find(proto);
      size_t ihs;
      if(itr != protoSrcCheckSummer.end() && (ihs = size_t(hdr->ihl * 4)) <= sz)
      {
        itr->second(hdr, buf + ihs, sz - ihs, oSrcIP, oDstIP);
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
