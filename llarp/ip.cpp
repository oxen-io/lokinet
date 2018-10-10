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
        byte_t, std::function< void(const ip_header *, byte_t *, size_t) > >
        protoDstCheckSummer = {
            // {RFC3022} says that IPv4 hdr isn't included in ICMP checksum calc
            // and that we don't need to modify it
            {// TCP
             6,
             [](const ip_header *hdr, byte_t *pkt, size_t sz) {
               auto hlen = size_t(hdr->ihl * 4);

               uint16_t *check = (uint16_t *)(pkt + hlen + 16);

               *check = deltachksum(*check, huint32_t{0}, huint32_t{0},
                                    xntohl(nuint32_t{hdr->saddr}),
                                    xntohl(nuint32_t{hdr->daddr}));
             }},
            {// UDP
             17,
             [](const ip_header *hdr, byte_t *pkt, size_t sz) {
               auto hlen = size_t(hdr->ihl * 4);

               uint16_t *check = (uint16_t *)(pkt + hlen + 16);
               if(*check != 0xFFff)
               {
                 if(*check == 0x0000)
                   return;  // don't change zero

                 *check = deltachksum(*check, huint32_t{0}, huint32_t{0},
                                      xntohl(nuint32_t{hdr->saddr}),
                                      xntohl(nuint32_t{hdr->daddr}));
                 if(*check == 0x0000)
                   *check = 0xFFff;
               }
               else
               {
                 // such checksum can mean 2 things: 0x0000 or 0xFFff
                 // we can only know by looking at data :<
                 if(hlen > sz)
                   return;  // malformed, bail out

                 auto oldcs = *check;

                 *check = 0;  // zero checksum before calculation

                 auto cs =
                     ipchksum(pkt + hlen, sz - hlen,
                              ipchksum_pseudoIPv4(nuint32_t{0}, nuint32_t{0},
                                                  17, sz - hlen));

                 auto mod_cs = deltachksum(cs, huint32_t{0}, huint32_t{0},
                                           xntohl(nuint32_t{hdr->saddr}),
                                           xntohl(nuint32_t{hdr->daddr}));

                 if(cs != 0x0000 && cs != 0xFFff)
                 {
                   // packet was bad - sabotage new checksum
                   mod_cs += cs - oldcs;
                 }
                 // 0x0000 is reserved for no checksum
                 if(mod_cs == 0x0000)
                   mod_cs = 0xFFff;
                 // put it in
                 *check = mod_cs;
               }
             }},

    };
    void
    IPv4Packet::UpdateChecksumsOnDst()
    {
      auto hdr = Header();

      // IPv4 checksum
      hdr->check = deltachksum(hdr->check, huint32_t{0}, huint32_t{0},
                               xntohl(nuint32_t{hdr->saddr}),
                               xntohl(nuint32_t{hdr->daddr}));

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

               *check = deltachksum(*check, xntohl(nuint32_t{hdr->saddr}),
                                    xntohl(nuint32_t{hdr->daddr}), huint32_t{0},
                                    huint32_t{0});
             }},
            {// UDP
             17,
             [](const ip_header *hdr, byte_t *pkt, size_t sz) {
               auto hlen = size_t(hdr->ihl * 4);

               uint16_t *check = (uint16_t *)(pkt + hlen + 16);
               if(*check != 0xFFff)
               {
                 if(*check == 0x0000)
                   return;  // don't change zero

                 *check = deltachksum(*check, xntohl(nuint32_t{hdr->saddr}),
                                      xntohl(nuint32_t{hdr->daddr}),
                                      huint32_t{0}, huint32_t{0});
                 if(*check == 0x0000)
                   *check = 0xFFff;
               }
               else
               {
                 // such checksum can mean 2 things: 0x0000 or 0xFFff
                 // we can only know by looking at data :<
                 if(hlen > sz)
                   return;  // malformed, bail out

                 auto oldcs = *check;

                 *check = 0;  // zero checksum before calculation

                 auto cs = ipchksum(
                     pkt + hlen, sz - hlen,
                     ipchksum_pseudoIPv4(nuint32_t{hdr->saddr},
                                         nuint32_t{hdr->daddr}, 17, sz - hlen));

                 auto mod_cs = deltachksum(cs, xntohl(nuint32_t{hdr->saddr}),
                                           xntohl(nuint32_t{hdr->daddr}),
                                           huint32_t{0}, huint32_t{0});

                 if(cs != 0x0000 && cs != 0xFFff)
                 {
                   // packet was bad - sabotage new checksum
                   mod_cs += cs - oldcs;
                 }
                 // 0x0000 is reserved for no checksum
                 if(mod_cs == 0x0000)
                   mod_cs = 0xFFff;
                 // put it in
                 *check = mod_cs;
               }
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
      hdr->check = deltachksum(hdr->check, xntohl(nuint32_t{hdr->saddr}),
                               xntohl(nuint32_t{hdr->daddr}), huint32_t{0},
                               huint32_t{0});
    }
  }  // namespace net
}  // namespace llarp
