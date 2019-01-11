#ifndef LLARP_IP_HPP
#define LLARP_IP_HPP

#include <ev/ev.h>
#include <net.hpp>
#include <util/buffer.h>
#include <util/time.hpp>

#ifndef _WIN32
// unix, linux
#include <sys/types.h>  // FreeBSD needs this for uchar for ip.h
#include <netinet/in.h>
#include <netinet/ip.h>
// anything not win32
struct ip_header
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
  unsigned int ihl : 4;
  unsigned int version : 4;
#elif __BYTE_ORDER == __BIG_ENDIAN
  unsigned int version : 4;
  unsigned int ihl : 4;
#else
#error "Please fix <bits/endian.h>"
#endif

#if defined(__linux__)
#define ip_version version
#endif
  uint8_t tos;
  uint16_t tot_len;
  uint16_t id;
  uint16_t frag_off;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t check;
  uint32_t saddr;
  uint32_t daddr;
};
#else
// windows nt
#include <winsock2.h>
typedef struct ip_hdr
{
  unsigned char
      ip_header_len : 4;  // 4-bit header length (in 32-bit words) normally=5
                          // (Means 20 Bytes may be 24 also)
  unsigned char version : 4;       // 4-bit IPv4 version
  unsigned char ip_tos;            // IP type of service
  unsigned short ip_total_length;  // Total length
  unsigned short ip_id;            // Unique identifier

  unsigned char ip_frag_offset : 5;  // Fragment offset field

  unsigned char ip_more_fragment : 1;
  unsigned char ip_dont_fragment : 1;
  unsigned char ip_reserved_zero : 1;

  unsigned char ip_frag_offset1;  // fragment offset

  unsigned char ip_ttl;        // Time to live
  unsigned char ip_protocol;   // Protocol(TCP,UDP etc)
  unsigned short ip_checksum;  // IP checksum
  unsigned int ip_srcaddr;     // Source address
  unsigned int ip_destaddr;    // Source address
} IPV4_HDR;
#define ip_header IPV4_HDR
#define saddr ip_srcaddr
#define daddr ip_destaddr
#define check ip_checksum
#define ihl ip_header_len
#define protocol ip_protocol
#define frag_off ip_frag_offset

#endif

#include <memory>

namespace llarp
{
  namespace net
  {
    struct IPv4Packet
    {
      static constexpr size_t MaxSize = 1500;
      llarp_time_t timestamp;
      size_t sz;
      byte_t buf[MaxSize];

      llarp_buffer_t
      Buffer();

      llarp_buffer_t
      ConstBuffer() const;

      bool
      Load(llarp_buffer_t buf);

      struct GetTime
      {
        llarp_time_t
        operator()(const IPv4Packet& pkt) const
        {
          return pkt.timestamp;
        }
      };

      struct PutTime
      {
        llarp_ev_loop* loop;
        PutTime(llarp_ev_loop* evloop) : loop(evloop)
        {
        }
        void
        operator()(IPv4Packet& pkt) const
        {
          pkt.timestamp = llarp_ev_loop_time_now_ms(loop);
        }
      };

      struct GetNow
      {
        llarp_ev_loop* loop;
        GetNow(llarp_ev_loop* evloop) : loop(evloop)
        {
        }
        llarp_time_t
        operator()() const
        {
          return llarp_ev_loop_time_now_ms(loop);
        }
      };

      struct CompareSize
      {
        bool
        operator()(const IPv4Packet& left, const IPv4Packet& right)
        {
          return left.sz < right.sz;
        }
      };

      struct CompareOrder
      {
        bool
        operator()(const IPv4Packet& left, const IPv4Packet& right)
        {
          return left.timestamp < right.timestamp;
        }
      };

      inline ip_header*
      Header()
      {
        return (ip_header*)&buf[0];
      }

      inline const ip_header*
      Header() const
      {
        return (ip_header*)&buf[0];
      }

      inline huint32_t
      src()
      {
        return huint32_t{ntohl(Header()->saddr)};
      }

      inline huint32_t
      dst()
      {
        return huint32_t{ntohl(Header()->daddr)};
      }

      inline void
      src(huint32_t ip)
      {
        Header()->saddr = htonl(ip.h);
      }

      inline void
      dst(huint32_t ip)
      {
        Header()->daddr = htonl(ip.h);
      }

      // update ip packet (after packet gets out of network)
      void
      UpdateIPv4PacketOnDst(huint32_t newSrcIP, huint32_t newDstIP);

      // update ip packet (before packet gets inserted into network)
      void
      UpdateIPv4PacketOnSrc();
    };

  }  // namespace net
}  // namespace llarp

#endif
