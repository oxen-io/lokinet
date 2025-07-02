#include "ip_packet.hpp"

#include <llarp/net/policy.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/logging/buffer.hpp>
#include <llarp/util/time.hpp>

#include <oxenc/endian.h>

#include <cstddef>
#include <utility>

namespace llarp
{
    static auto logcat = log::Cat("ip_packet");

    // constexpr auto IP_CSUM_OFF = offsetof(struct ip_header, checksum);
    // constexpr auto IP_DST_OFF = offsetof(struct ip_header, dest);
    // constexpr auto IP_SRC_OFF = offsetof(struct ip_header, src);
    // constexpr auto IP_PROTO_OFF = offsetof(struct ip_header, protocol);
    // constexpr auto TCP_DATA_OFF = oxenc::host_to_big<uint32_t>(0xF0000000);
    constexpr auto TCP_CSUM_OFF = offsetof(struct tcp_header, checksum);
    constexpr auto UDP_CSUM_OFF = offsetof(struct udp_header, checksum);
    // auto TCP_DATA_OFFSET = htonl(0xF0000000);
    // constexpr auto IS_PSEUDO = 0x10;

    IPPacket::IPPacket(size_t sz)
    {
        if (sz and sz < MIN_PACKET_SIZE)
            throw std::invalid_argument{"Buffer size is too small for an IP packet!"};
        _buf.resize(sz, std::byte{0});
        _init_internals();
    }

    IPPacket::IPPacket(std::vector<std::byte>&& data) : _buf{std::move(data)} { _init_internals(); }

    IPPacket::IPPacket(std::span<const std::byte> buf)
    {
        if (buf.size() < MIN_PACKET_SIZE)
            throw std::invalid_argument{"Buffer data is too small for an IP packet!"};

        _buf.resize(buf.size());
        std::memcpy(_buf.data(), buf.data(), buf.size());

        _init_internals();
    }

    std::optional<IPPacket> IPPacket::try_making(std::span<const std::byte> buf)
    {
        std::optional<IPPacket> ret;
        try
        {
            ret.emplace(buf);
        }
        catch (const std::invalid_argument& e)
        {
            log::trace(logcat, "Invalid IP packet: {}", e.what());
        }
        return ret;
    }

    static constexpr uint8_t v4_header_version = 4;
    static constexpr uint8_t v6_header_version = 6;

    void IPPacket::_init_internals()
    {
        if (_buf.empty())
            return;

        const auto* header = reinterpret_cast<ip_header*>(data());
        const auto* v6_header = reinterpret_cast<ipv6_header*>(data());

        _is_v4 = header->version == v4_header_version;
        _is_v6 = header->version == v6_header_version;
        assert(!(_is_v4 && _is_v6));
        if (!_is_v4 && !_is_v6)
            return;  // Not an IP packet!

        uint16_t pkt_len;
        if (_is_v4)
        {
            _proto = net::IPProtocol{header->protocol};
            pkt_len = oxenc::big_to_host(header->total_len);
            _header_len = 4 * header->header_len;
            _payload_len = pkt_len - _header_len;
        }
        else
        {
            _proto = net::IPProtocol{v6_header->protocol};
            _header_len = 40;
            _payload_len = oxenc::big_to_host(v6_header->payload_len);
            pkt_len = _payload_len + _header_len;
        }

        if (pkt_len != size())
            throw std::invalid_argument{
                "Invalid IP packet size: header implies {}B, but packet is {}B"_format(pkt_len, size())};

        uint16_t src_port = 0, dest_port = 0;
        if ((_proto == net::IPProtocol::UDP || _proto == net::IPProtocol::TCP) && _payload_len >= 4)
        {
            src_port = oxenc::load_big_to_host<uint16_t>(data() + _header_len);
            dest_port = oxenc::load_big_to_host<uint16_t>(data() + _header_len + 2);
        }

        if (_is_v4)
        {
            _src_addr = quic::Address{ipv4{oxenc::big_to_host(header->src)}, src_port};
            _dst_addr = quic::Address{ipv4{oxenc::big_to_host(header->dest)}, dest_port};
        }
        else
        {
            _src_addr = quic::Address{ipv6{v6_header->src}, src_port};
            _dst_addr = quic::Address{ipv6{v6_header->dest}, dest_port};
        }
        log::trace(logcat, "IP packet init: proto={}, src={}, dest={}", _proto, _src_addr, _dst_addr);
    }

    std::span<const std::byte> IPPacket::udp_data()
    {
        if (_proto != net::IPProtocol::UDP || _payload_len < 8)
            return {};

        auto hlen = _header_len + 8;
        return {reinterpret_cast<const std::byte*>(data()) + hlen, size() - hlen};
    }

    void IPPacket::update_ipv4_address(const ipv4& src, const ipv4& dst)
    {
        log::trace(logcat, "Setting new source ({}) and destination ({}) IPs", src, dst);

        auto _header = reinterpret_cast<ip_header*>(data());
        if (auto ihs = size_t(_header->header_len * 4), sz = size(); ihs <= sz)
        {
            auto* payload = data() + ihs;
            auto payload_size = sz - ihs;
            auto frag_off = size_t(oxenc::big_to_host(_header->frag_off) & 0x1Fff) * 8;

            auto ip_proto = static_cast<net::IPProtocol>(_header->protocol);
            switch (ip_proto)
            {
                case net::IPProtocol::TCP:
                    if (frag_off <= TCP_CSUM_OFF && payload_size >= TCP_CSUM_OFF - frag_off + 2)
                    {
                        auto* tcp_hdr = reinterpret_cast<tcp_header*>(payload);
                        tcp_hdr->checksum =
                            utils::ipv4_tcp_checksum_diff(tcp_hdr->checksum, _header->src, _header->dest, src, dst);
                    }
                    break;
                case net::IPProtocol::UDP:
                case net::IPProtocol::UDP_LITE:  // UDP-Lite - same checksum place, same 0->0xFFff condition
                    if (frag_off <= UDP_CSUM_OFF && payload_size >= UDP_CSUM_OFF + 2)
                    {
                        auto* udp_hdr = reinterpret_cast<udp_header*>(payload);
                        udp_hdr->checksum =
                            utils::ipv4_udp_checksum_diff(udp_hdr->checksum, _header->src, _header->dest, src, dst);
                    }
                    break;
                case net::IPProtocol::DCCP:
                    if (frag_off <= UDP_CSUM_OFF || payload_size >= UDP_CSUM_OFF - frag_off + 2)
                    {
                        auto* tcp_hdr = reinterpret_cast<tcp_header*>(payload);
                        tcp_hdr->checksum =
                            utils::ipv4_tcp_checksum_diff(tcp_hdr->checksum, _header->src, _header->dest, src, dst);
                    }
                    break;
                default:
                    // do nothing (or not implemented)
                    break;
            }
        }

        _header->checksum = utils::ipv4_checksum_diff(_header->checksum, _header->src, _header->dest, src, dst);

        // set new IP addresses
        _header->src = oxenc::host_to_big(src.addr);
        _header->dest = oxenc::host_to_big(dst.addr);

        _src_addr.set_addr(reinterpret_cast<in_addr*>(&_header->src));
        _dst_addr.set_addr(reinterpret_cast<in_addr*>(&_header->dest));
    }

    void IPPacket::update_ipv6_address(const ipv6& src, const ipv6& dst, std::optional<uint32_t> flowlabel)
    {
        const auto sz = size();
        // XXX should've been checked at upper level?
        if (sz <= sizeof(ipv6_header))
            return;

        auto& hdr = v6_header();
        if (flowlabel.has_value())
            hdr.flowlabel(*flowlabel);

        // IPv6 address
        hdr.src = static_cast<in6_addr>(src);
        hdr.dest = static_cast<in6_addr>(dst);

        // TODO IPv6 header options
        auto* pld = reinterpret_cast<const uint8_t*>(data()) + sizeof(ipv6_header);
        auto psz = sz - sizeof(ipv6_header);

        size_t fragoff = 0;
        auto nextproto = hdr.protocol;
        for (;;)
        {
            switch (nextproto)
            {
                case 0:   // Hop-by-Hop Options
                case 43:  // Routing Header
                case 60:  // Destination Options
                {
                    nextproto = pld[0];
                    auto addlen = (size_t(pld[1]) + 1) * 8;
                    if (psz < addlen)
                        return;
                    pld += addlen;
                    psz -= addlen;
                    break;
                }

                case 44:  // Fragment Header
                    /*
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           |  Next Header  |   Reserved    |      Fragment Offset    |Res|M|
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           |                         Identification                        |
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                     */
                    nextproto = pld[0];
                    fragoff = (uint16_t(pld[2]) << 8) | (uint16_t(pld[3]) & 0xFC);
                    if (psz < 8)
                        return;
                    pld += 8;
                    psz -= 8;

                    // jump straight to payload processing
                    if (fragoff != 0)
                        goto endprotohdrs;
                    break;

                default:
                    goto endprotohdrs;
            }
        }
    endprotohdrs:

        uint16_t chksumoff{0};
        uint16_t chksum{0};

        bool is_udp = false;

        switch (static_cast<net::IPProtocol>(nextproto))
        {
            case net::IPProtocol::TCP:
                chksumoff = 16;
                [[fallthrough]];
            case net::IPProtocol::DCCP:
                chksum = tcp_checksum_ipv6(&hdr.src, &hdr.dest, hdr.payload_len, 0);
                // ones-complement addition fo 0xFFff is 0; this is verboten
                if (chksum == 0xFFff)
                    chksum = 0x0000;

                chksumoff = chksumoff == 16 ? 16 : 6;
                break;
            case net::IPProtocol::UDP:
            case net::IPProtocol::UDP_LITE:  // UDP-Lite - same checksum place, same 0->0xFFff condition
                chksum = udp_checksum_ipv6(&hdr.src, &hdr.dest, hdr.payload_len, 0);
                is_udp = true;
                break;
            default:
                // do nothing
                break;
        }

        auto check = is_udp ? (uint16_t*)(pld + 6) : (uint16_t*)(pld + chksumoff - fragoff);

        *check = chksum;

        _init_internals();
    }

    std::optional<IPPacket> IPPacket::make_icmp_unreachable() const
    {
        if (is_ipv4())
        {
            const auto& header = *reinterpret_cast<const ip_header*>(data());
            auto ip_hdr_sz = header.header_len * 4;
            size_t pkt_size = (ICMP_HEADER_SIZE + ip_hdr_sz) * 2;

            if (pkt_size < MIN_PACKET_SIZE)
                return std::nullopt;

            IPPacket pkt{*this};

            pkt.header().version = 0x04;
            pkt.header().header_len = 0x05;
            pkt.header().service_type = 0;
            pkt.header().checksum = 0;
            pkt.header().total_len = ntohs(pkt_size);
            pkt.header().src = header.dest;
            pkt.header().dest = header.src;
            pkt.header().protocol = 1;  // ICMP
            pkt.header().ttl = header.ttl;
            pkt.header().frag_off = oxenc::host_to_big<uint16_t>(0b01000000'00000000);

            uint8_t* itr = reinterpret_cast<uint8_t*>(pkt.data()) + ip_hdr_sz;
            uint8_t* icmp_begin = itr;  // type 'destination unreachable'
            *itr++ = 3;

            // code 'Destination host unknown error'
            *itr++ = 7;

            // checksum + unused
            oxenc::write_host_as_big<uint32_t>(0, itr);
            auto* checksum = (uint16_t*)itr;
            itr += 4;

            // next hop mtu is ignored but let's put something here anyways just in case tm
            oxenc::write_host_as_big<uint16_t>(1500, itr);
            itr += 2;

            // copy ip header and first 8 bytes of datagram for icmp reject
            std::memcpy(itr, _buf.data(), ip_hdr_sz + ICMP_HEADER_SIZE);
            itr += ip_hdr_sz + ICMP_HEADER_SIZE;

            // calculate checksum of ip header
            pkt.header().checksum = utils::ip_checksum(reinterpret_cast<const uint8_t*>(pkt.data()), ip_hdr_sz);

            // calculate icmp checksum
            *checksum = utils::ip_checksum(icmp_begin, std::distance(icmp_begin, itr));

            log::debug(logcat, "ICMP unreachable pkt configured");
            return pkt;
        }

        // TODO FIXME: ipv6

        return std::nullopt;
    }

    // TODO: ipv6
    std::vector<std::byte> IPPacket::make_udp_packet(
        const quic::Address& src, const quic::Address& dest, std::span<const std::byte> payload)
    {
        std::vector<std::byte> pkt;
        pkt.resize(sizeof(ip_header) + sizeof(udp_header) + payload.size());
        auto* data = pkt.data();
        data[1] = std::byte{0};  // DSCP and ECN
        auto* ip_hdr = reinterpret_cast<ip_header*>(data);
        data += sizeof(ip_header);
        auto* udp_hdr = reinterpret_cast<udp_header*>(data);
        data += sizeof(udp_header);
        std::memcpy(data, payload.data(), payload.size());

        ip_hdr->version = 4;
        ip_hdr->header_len = 5;
        ip_hdr->total_len = htons(sizeof(ip_header) + sizeof(udp_header) + payload.size());
        ip_hdr->protocol = static_cast<uint8_t>(net::IPProtocol::UDP);  // udp
        ip_hdr->ttl = 64;
        ip_hdr->frag_off = oxenc::host_to_big<uint16_t>(0b01000000'00000000);

        ip_hdr->src = oxenc::host_to_big(src.to_ipv4().addr);
        ip_hdr->dest = oxenc::host_to_big(dest.to_ipv4().addr);
        ip_hdr->checksum = utils::ip_checksum(reinterpret_cast<uint8_t*>(ip_hdr), sizeof(ip_header));

        udp_hdr->src = oxenc::host_to_big(src.port());
        udp_hdr->dest = oxenc::host_to_big(dest.port());
        udp_hdr->len = oxenc::host_to_big<uint16_t>(payload.size() + sizeof(udp_header));
        udp_hdr->checksum = 0;  // FIXME: does this matter?  old lokinet set 0

        return pkt;
    }

    quic::Packet IPPacket::make_netpkt()
    {
        quic::Packet p{
            quic::Path{_src_addr, _dst_addr}, {reinterpret_cast<const std::byte*>(_buf.data()), _buf.size()}};
        p.ensure_owned_data();
        return p;
    }

    std::string IPPacket::info_line() const
    {
        return "IPPacket:[ type:{} | src:{} | dest:{} | size:{} ]"_format(
            ip_protocol_name(_proto), _src_addr, _dst_addr, size());
    }

}  // namespace llarp
