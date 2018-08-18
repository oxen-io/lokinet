#ifndef LLARP_IP_HPP
#define LLARP_IP_HPP
#include <llarp/buffer.h>
#include <llarp/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <memory>
#ifndef __linux__
// slightly different type names on sunos
#define iphdr ip
#define saddr ip_src.s_addr
#define daddr ip_dst.s_addr
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

      struct GetTime
      {
        llarp_time_t
        operator()(const IPv4Packet* pkt) const
        {
          return pkt->timestamp;
        }
      };

      struct PutTime
      {
        void
        operator()(IPv4Packet* pkt) const
        {
          pkt->timestamp = llarp_time_now_ms();
        }
      };

      struct CompareOrder
      {
        bool
        operator()(const std::unique_ptr< IPv4Packet >& left,
                   const std::unique_ptr< IPv4Packet >& right)
        {
          return left->timestamp < right->timestamp;
        }
      };

      iphdr*
      Header()
      {
        return (iphdr*)buf;
      }

      const iphdr*
      Header() const
      {
        return (iphdr*)buf;
      }

      uint32_t&
      src()
      {
        return Header()->saddr;
      }

      uint32_t&
      dst()
      {
        return Header()->daddr;
      }

      const uint32_t&
      src() const
      {
        return Header()->saddr;
      }

      const uint32_t&
      dst() const
      {
        return Header()->daddr;
      }

      /// put the payload of an ip packet
      /// recalculate all fields
      /// return true on success
      /// return false if the payload doesn't fit
      bool
      PutPayload(llarp_buffer_t buf);
    };

    /// parse an ipv4 packet
    /// returns nullptr if invalid data
    /// copies buffer into return value
    std::unique_ptr< IPv4Packet >
    ParseIPv4Packet(const void* buf, size_t sz);

  }  // namespace net
}  // namespace llarp

#endif
