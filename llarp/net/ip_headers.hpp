#pragma once

#include "utils.hpp"

#include <netinet/ip6.h>

namespace llarp
{
    struct ip_header
    {
#if __BYTE_ORDER == __LITTLE_ENDIAN
        uint8_t header_len : 4;
        uint8_t version : 4;
#else
        uint8_t version : 4;
        uint8_t header_len : 4;
#endif
        uint8_t service_type;
        uint16_t total_len;  // entire packet size
        uint16_t id;
        uint16_t frag_off;  // fragmentation offset
        uint8_t ttl;
        uint8_t protocol;
        uint16_t checksum;
        uint32_t src;
        uint32_t dest;
    };

    static_assert(sizeof(ip_header) == 20);

    struct ipv6_header
    {
#if __BYTE_ORDER == __LITTLE_ENDIAN
        uint8_t tclass_hi : 4;
        uint8_t version : 4;
        uint8_t flow_hi : 4;
        uint8_t tclass_lo : 4;
#else
        uint8_t version : 4;
        uint8_t tclass_hi : 4;
        uint8_t tclass_lo : 4;
        uint8_t flow_hi : 4;
#endif
        uint16_t flow_lo;
        uint16_t payload_len;
        uint8_t protocol;
        uint8_t hoplimit;
        in6_addr src;
        in6_addr dest;

        /// Returns the traffic class value
        constexpr uint8_t tclass() const { return (tclass_hi << 4) | tclass_lo; }

        /// Sets a traffic class value
        constexpr void tclass(uint8_t tcl)
        {
            tclass_hi = (tcl >> 4);
            tclass_lo = tcl % 0xf;
        }

        /// Extracts the host-order decoded flowlabel
        constexpr uint32_t flowlabel() const { return (flow_hi << 16) | oxenc::big_to_host(flow_lo); }

        /// Sets a host-order flowlabel.
        constexpr void flowlabel(uint32_t label)
        {
            flow_hi = (label >> 16) & 0x0f;
            flow_lo = oxenc::host_to_big(label & 0xffff);
        }
    };

    static_assert(sizeof(ipv6_header) == 40);

    /** TODO: for mobile ipv6, implement ipv6 routing headers
     */

    enum class TCPFLAG : uint8_t
    {
        FIN = 0x01,
        SYN = 0x02,
        RST = 0x04,
        PUSH = 0x08,
        ACK = 0x10,
        URG = 0x20
    };

    struct tcp_header
    {
        uint16_t src;    // src addr or port
        uint16_t dest;   // dst addr or port
        uint32_t seqno;  // sequence number
        uint32_t ack;    // ack number
#if __BYTE_ORDER == __LITTLE_ENDIAN
        uint8_t : 4;           // unused/reserved
        uint8_t data_off : 4;  // data offset
#else
        uint8_t data_off : 4;  // data offset
        uint8_t : 4;           // unused/reserved
#endif
        uint8_t flags;
        uint16_t window;
        uint16_t checksum;
        uint16_t urg_ptr;  // urgent ptr
    };
    static_assert(sizeof(tcp_header) == 20);

    struct udp_header
    {
        uint16_t src;
        uint16_t dest;
        uint16_t len;  // datagram length
        uint16_t checksum;
    };
    static_assert(sizeof(udp_header) == 8);

}  // namespace llarp
