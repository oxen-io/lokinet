#pragma once

#include <llarp/address/ip_range.hpp>

#include <oxenc/bt.h>

#include <set>

namespace llarp
{
    struct IPPacket;

    enum class protocol_flag : uint8_t
    {
        NONE = 0,
        EXIT = 1 << 0,         // This client is configured to route (some) exit traffic
        QUIC_TUNNEL = 1 << 1,  // This client supports QUIC tunnel (currently always enabled)
        IPV4 = 1 << 2,         // This client support Lokinet raw IPv4 packets (always set for non-embedded clients)
        IPV6 = 1 << 3,         // This client support Lokinet raw IPv6 packets (not yet supported)
    };
    inline constexpr protocol_flag operator&(protocol_flag a, protocol_flag b)
    {
        return static_cast<protocol_flag>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }
    inline constexpr protocol_flag& operator&=(protocol_flag& a, protocol_flag b)
    {
        a = a & b;
        return a;
    }
    inline constexpr protocol_flag operator|(protocol_flag a, protocol_flag b)
    {
        return static_cast<protocol_flag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }
    inline constexpr protocol_flag& operator|=(protocol_flag& a, protocol_flag b)
    {
        a = a | b;
        return a;
    }
    // Returns true if `flags` contains all of the set flags in `contains`
    inline constexpr bool has_flag(protocol_flag flags, protocol_flag contains)
    {
        return (flags & contains) == contains;
    }

    /** proto_mask
        - When negotiating sessions and advertising client contacts, all flags can be used
        - When prepended to a datagram, only 2 bits are needed for 4 configurations
            - host, standard = 00
            - host, quictun = 01
            - exit, standard = 10
            - exit, quictun = 11

        FIXME: What?  I think this is obsolete.
    */
    inline constexpr protocol_flag proto_mask = protocol_flag::EXIT | protocol_flag::QUIC_TUNNEL;

    std::string to_string(protocol_flag p);

    namespace net
    {
        enum class IPProtocol : uint8_t
        {
            ICMP = 0x01,
            IGMP = 0x02,
            IPIP = 0x04,
            TCP = 0x06,
            UDP = 0x11,
            DCCP = 0x21,
            GRE = 0x2F,
            ICMP6 = 0x3A,
            OSPF = 0x59,
            PGM = 0x71,
            UDP_LITE = 0x88,
        };

        inline constexpr std::string_view ip_protocol_name(IPProtocol p)
        {
            switch (p)
            {
                case IPProtocol::ICMP:
                    return "ICMP"sv;
                case IPProtocol::IGMP:
                    return "IGMP"sv;
                case IPProtocol::IPIP:
                    return "IPIP"sv;
                case IPProtocol::TCP:
                    return "TCP"sv;
                case IPProtocol::UDP:
                    return "UDP"sv;
                case IPProtocol::DCCP:
                    return "DCCP"sv;
                case IPProtocol::GRE:
                    return "GRE"sv;
                case IPProtocol::ICMP6:
                    return "ICMP6"sv;
                case IPProtocol::OSPF:
                    return "OSPF"sv;
                case IPProtocol::PGM:
                    return "PGM"sv;
                case IPProtocol::UDP_LITE:
                    return "UDP-Lite"sv;
            }
            return "<UNKNOWN>"sv;
        }

        /// information about an IP protocol
        struct ProtocolInfo
        {
            /// ip protocol of this protocol
            IPProtocol protocol;

            /// the layer 4 port (TCP and UDP)
            std::optional<uint16_t> port{std::nullopt};

            ProtocolInfo() = default;
            ProtocolInfo(oxenc::bt_list_consumer&& enc);

            // Constructs from a user-supplied value typically from the config such as "tcp" or
            // "udp/53" or "0x69".  Throws on invalid input.
            static ProtocolInfo from_config(std::string_view config_input);

            void bt_encode(oxenc::bt_list_producer&& btlp) const;

            // Compares packet protocol with protocol info
            bool matches_packet_proto(const IPPacket& pkt) const;

            auto operator<=>(const ProtocolInfo& other) const = default;
            bool operator==(const ProtocolInfo& other) const = default;
        };

        /// information about what exit traffic an endpoint will carry
        struct ExitPolicy
        {
            /// ranges that are allowed.  If empty, allow none.
            std::vector<ipv4_range> ranges;
            std::vector<ipv6_range> ranges_v6;

            /// protocols that are explicity allowed.  If empty, allow all.
            std::set<ProtocolInfo> protocols;

            bool empty() const { return ranges.empty() and protocols.empty(); }

            void bt_encode(oxenc::bt_dict_producer&& btdp) const;

            void bt_decode(oxenc::bt_dict_consumer&& btdc);

            bool bt_decode(std::string_view buf);

            // Verifies if IPPacket traffic is allowed; return true/false
            bool allow_ip_traffic(const IPPacket& pkt) const;

            bool operator==(const ExitPolicy& other) const = default;
        };
    }  // namespace net
}  // namespace llarp

namespace fmt
{
    template <>
    struct formatter<llarp::net::IPProtocol, char> : formatter<std::string_view>
    {
        template <typename FormatContext>
        auto format(llarp::net::IPProtocol p, FormatContext& ctx) const
        {
            return formatter<std::string_view>::format(ip_protocol_name(p), ctx);
        }
    };
}  // namespace fmt
