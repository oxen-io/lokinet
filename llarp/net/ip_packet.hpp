#pragma once

#include "ip_headers.hpp"
#include "policy.hpp"

#include <llarp/util/buffer.hpp>
#include <llarp/util/formattable.hpp>
#include <llarp/util/time.hpp>

#include <oxen/quic/address.hpp>
#include <oxen/quic/udp.hpp>

namespace llarp
{
    inline constexpr size_t MAX_PACKET_SIZE{1500};
    inline constexpr size_t MIN_PACKET_SIZE{20};

    struct IPPacket;

    using net_pkt_hook = std::function<void(quic::Packet&& pkt)>;
    using ip_pkt_hook = std::function<void(IPPacket)>;

    /** IPPacket
        This class encapsulates the functionalities and attributes required for data transmission between the local
        lokinet instance and the surrounding IP landscape. As data enters lokinet from the device/internet/etc, it is
        transmitted across the network as a quic::Packet via QUIC. As it exits lokinet to the device/internet/etc, it
        is constructed into an IPPacket.

        This allows for necessary functionalities at the junction that data is entering and exiting the local lokinet
        instance. For example

    */
    struct IPPacket
    {
      private:
        std::vector<std::byte> _buf;

        quic::Address _src_addr;
        quic::Address _dst_addr;

        bool _is_v4;
        uint8_t _header_len;
        uint16_t _payload_len;

        net::IPProtocol _proto{};

        void _init_internals();

      public:
        IPPacket() : IPPacket{size_t{0}} {}
        explicit IPPacket(size_t sz);
        explicit IPPacket(std::vector<std::byte>&& data);
        explicit IPPacket(std::span<const std::byte> buf);

        // Is this gross thing really needed?
        static std::optional<IPPacket> try_making(std::span<const std::byte> buf);

        quic::Packet make_netpkt();

        // TESTNET: debug methods
        // uint16_t checksum() const { return _is_v4 ? header()->checksum : 0; }

        bool is_ipv4() const { return _is_v4; }

        net::IPProtocol protocol() const { return _proto; }

        const quic::Address& source() const { return _src_addr; }

        uint16_t source_port() { return source().port(); }

        ipv4 source_ipv4() { return _src_addr.to_ipv4(); }

        ipv6 source_ipv6() { return _src_addr.to_ipv6(); }

        const quic::Address& destination() const { return _dst_addr; }

        uint16_t dest_port() { return destination().port(); }

        ipv4 dest_ipv4() const { return _dst_addr.to_ipv4(); }

        ipv6 dest_ipv6() const { return _dst_addr.to_ipv6(); }

        ip_header& header() { return *reinterpret_cast<ip_header*>(data()); }
        const ip_header& header() const { return *reinterpret_cast<const ip_header*>(data()); }

        ipv6_header& v6_header() { return *reinterpret_cast<ipv6_header*>(data()); }
        const ipv6_header& v6_header() const { return *reinterpret_cast<const ipv6_header*>(data()); }

        std::span<const std::byte> udp_data();

        void clear_addresses()
        {
            if (_is_v4)
                return update_ipv4_address(ipv4{}, ipv4{});
            return update_ipv6_address(ipv6{}, ipv6{});
        }

        void update_ipv4_address(const ipv4& src, const ipv4& dst);

        void update_ipv6_address(const ipv6& src, const ipv6& dst, std::optional<uint32_t> flowlabel = std::nullopt);

        std::optional<IPPacket> make_icmp_unreachable() const;

        static std::vector<std::byte> make_udp_packet(
            const quic::Address& src, const quic::Address& dest, std::span<const std::byte> payload);

        std::byte* data() { return _buf.data(); }
        const std::byte* data() const { return _buf.data(); }

        size_t size() const { return _buf.size(); }

        std::span<std::byte> span() { return _buf; }
        std::span<const std::byte> span() const { return _buf; }

        bool empty() const { return _buf.empty(); }

        std::string info_line() const;
    };

}  // namespace llarp
