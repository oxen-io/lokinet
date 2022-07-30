#pragma once

#include <llarp/ev/ev.hpp>
#include "net.hpp"
#include <llarp/util/buffer.hpp>
#include <llarp/util/time.hpp>
#include <memory>
#include <llarp/service/protocol_type.hpp>
#include <utility>

namespace llarp::net
{
  template <bool is_little_endian>
  struct ip_header_le
  {
    unsigned int ihl : 4;
    unsigned int version : 4;
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

  template <>
  struct ip_header_le<false>
  {
    unsigned int version : 4;
    unsigned int ihl : 4;
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

  using ip_header = ip_header_le<oxenc::little_endian>;

  template <bool little>
  struct ipv6_header_preamble_le
  {
    unsigned char pad_small : 4;
    unsigned char version : 4;
    uint8_t pad[3];
  };

  template <>
  struct ipv6_header_preamble_le<false>
  {
    unsigned char version : 4;
    unsigned char pad_small : 4;
    uint8_t pad[3];
  };

  struct ipv6_header
  {
    union
    {
      ipv6_header_preamble_le<oxenc::little_endian> preamble;
      uint32_t flowlabel;
    } preamble;

    uint16_t payload_len;
    uint8_t proto;
    uint8_t hoplimit;
    in6_addr srcaddr;
    in6_addr dstaddr;
    llarp::nuint32_t
    FlowLabel() const;

    /// put 20 bit truncated flow label network order
    void
    FlowLabel(llarp::nuint32_t label);
  };

  /// "well known" ip protocols
  /// TODO: extend this to non "well known values"
  enum class IPProtocol : uint8_t
  {
    ICMP = 0x01,
    IGMP = 0x02,
    IPIP = 0x04,
    TCP = 0x06,
    UDP = 0x11,
    GRE = 0x2F,
    ICMP6 = 0x3A,
    OSFP = 0x59,
    PGM = 0x71,
  };

  /// get string representation of this protocol
  /// throws std::invalid_argument if we don't know the name of this ip protocol
  std::string
  IPProtocolName(IPProtocol proto);

  /// parse a string to an ip protocol
  /// throws std::invalid_argument if cannot be parsed
  IPProtocol
  ParseIPProtocol(std::string data);

  /// an Packet
  struct IPPacket
  {
    static constexpr size_t MaxSize = 1500;
    llarp_time_t timestamp;
    size_t sz;

    alignas(ip_header) byte_t buf[MaxSize];

    static IPPacket
    UDP(nuint32_t srcaddr,
        nuint16_t srcport,
        nuint32_t dstaddr,
        nuint16_t dstport,
        const llarp_buffer_t& data);

    ManagedBuffer
    Buffer();

    ManagedBuffer
    ConstBuffer() const;

    bool
    Load(const llarp_buffer_t& buf);

    struct CompareSize
    {
      bool
      operator()(const IPPacket& left, const IPPacket& right)
      {
        return left.sz < right.sz;
      }
    };

    struct CompareOrder
    {
      bool
      operator()(const IPPacket& left, const IPPacket& right)
      {
        return left.timestamp < right.timestamp;
      }
    };

    inline ip_header*
    Header()
    {
      return reinterpret_cast<ip_header*>(&buf[0]);
    }

    inline const ip_header*
    Header() const
    {
      return reinterpret_cast<const ip_header*>(&buf[0]);
    }

    inline ipv6_header*
    HeaderV6()
    {
      return reinterpret_cast<ipv6_header*>(&buf[0]);
    }

    inline const ipv6_header*
    HeaderV6() const
    {
      return reinterpret_cast<const ipv6_header*>(&buf[0]);
    }

    inline int
    Version() const
    {
      return Header()->version;
    }

    inline bool
    IsV4() const
    {
      return Version() == 4;
    }

    inline bool
    IsV6() const
    {
      return Version() == 6;
    }

    inline service::ProtocolType
    ServiceProtocol() const
    {
      if (IsV4())
        return service::ProtocolType::TrafficV4;
      if (IsV6())
        return service::ProtocolType::TrafficV6;

      return service::ProtocolType::Control;
    }

    huint128_t
    srcv6() const;

    huint128_t
    dstv6() const;

    huint32_t
    srcv4() const;

    huint32_t
    dstv4() const;

    huint128_t
    src4to6() const;

    huint128_t
    dst4to6() const;

    huint128_t
    src4to6Lan() const;

    huint128_t
    dst4to6Lan() const;

    /// get destination port if applicable
    std::optional<nuint16_t>
    DstPort() const;

    /// get source port if applicable
    std::optional<nuint16_t>
    SrcPort() const;

    /// get pointer and size of layer 4 data
    std::optional<std::pair<const char*, size_t>>
    L4Data() const;

    void
    UpdateIPv4Address(nuint32_t src, nuint32_t dst);

    void
    UpdateIPv6Address(
        huint128_t src, huint128_t dst, std::optional<nuint32_t> flowlabel = std::nullopt);

    /// set addresses to zero and recacluate checksums
    void
    ZeroAddresses(std::optional<nuint32_t> flowlabel = std::nullopt);

    /// zero out source address
    void
    ZeroSourceAddress(std::optional<nuint32_t> flowlabel = std::nullopt);

    /// make an ip packet that will close or reject whatever upper layer sent it
    /// makes a tcp rst packet for tcp, otherwise an icmp unreachble packet
    /// returns nullopt if not implemented or applicable for this packet
    std::optional<IPPacket>
    MakeReject() const;
  };

  /// generate ip checksum
  uint16_t
  ipchksum(const byte_t* buf, size_t sz, uint32_t sum = 0);

}  // namespace llarp::net
