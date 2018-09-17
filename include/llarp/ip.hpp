#ifndef LLARP_IP_HPP
#define LLARP_IP_HPP
#include <llarp/buffer.h>
#include <llarp/time.h>
#include <llarp/net.hpp>
#ifdef _WIN32
#include <winsock2.h>
#endif
struct iphdr
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned int ihl:4;
    unsigned int version:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
    unsigned int version:4;
    unsigned int ihl:4;
#else
# error "Please fix <bits/endian.h>"
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
