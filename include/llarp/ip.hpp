#ifndef LLARP_IP_HPP
#define LLARP_IP_HPP
#include <llarp/buffer.h>
#include <llarp/time.h>
#include <llarp/net.hpp>
#ifndef _WIN32
#include <netinet/in.h>
#include <netinet/ip.h>
#else
#include <winsock2.h>
// Apparently this does not seem to be located _anywhere_ in the windows sdk???
// -despair86
typedef struct ip_hdr
{
  unsigned char
      ip_header_len : 4;  // 4-bit header length (in 32-bit words) normally=5
                          // (Means 20 Bytes may be 24 also)
  unsigned char ip_version : 4;    // 4-bit IPv4 version
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
#define iphdr IPV4_HDR
#define saddr ip_srcaddr
#define daddr ip_destaddr
#define check ip_checksum
#define ihl ip_header_len
#endif
#include <memory>
#if !defined(__linux__) && !defined(_WIN32)
#define iphdr ip
#define saddr ip_src.s_addr
#define daddr ip_dst.s_addr
#define ip_version ip_v
#define check ip_sum
#define ihl ip_hl
#endif
#if defined(__linux__)
#define ip_version version
#endif

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
        void
        operator()(IPv4Packet& pkt) const
        {
          pkt.timestamp = llarp_time_now_ms();
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

      iphdr*
      Header()
      {
        return (iphdr*)&buf[0];
      }

      const iphdr*
      Header() const
      {
        return (iphdr*)&buf[0];
      }

      uint32_t
      src()
      {
        return ntohl(Header()->saddr);
      }

      uint32_t
      dst()
      {
        return ntohl(Header()->daddr);
      }

      void
      src(uint32_t ip)
      {
        Header()->saddr = htonl(ip);
      }

      void
      dst(uint32_t ip)
      {
        Header()->daddr = htonl(ip);
      }

      // update ip packet checksum
      void
      UpdateChecksum();
    };

  }  // namespace net
}  // namespace llarp

#endif
