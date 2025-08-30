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
    }

    IPPacket::IPPacket(std::vector<std::byte>&& data) : _buf{std::move(data)}
    {
        if (_buf.size() < MIN_PACKET_SIZE)
            throw std::invalid_argument{"Buffer data is too small for an IP packet!"};
    }

    IPPacket::IPPacket(std::span<const std::byte> buf)
    {
        if (buf.size() < MIN_PACKET_SIZE)
            throw std::invalid_argument{"Buffer data is too small for an IP packet!"};

        _buf.resize(buf.size());
        std::memcpy(_buf.data(), buf.data(), buf.size());
    }

    std::span<const std::byte> IPPacket::udp_data()
    {
        auto proto = protocol();
        if (proto != net::IPProtocol::UDP || payload_size() < 8)
            return {};
        return span().subspan(header_size() + 8);
    }

    void IPPacket::clear_addresses()
    {
        if (is_ipv4())
            update_ipv4_address(ipv4{}, ipv4{});
        else if (is_ipv6())
            update_ipv6_address(ipv6{}, ipv6{});
    }

    void IPPacket::update_ipv4_address(const ipv4& src, const ipv4& dst)
    {
        log::trace(logcat, "Setting new source ({}) and destination ({}) IPs", src, dst);

        auto& hdr = header();
        if (auto ihs = size_t(hdr.header_len * 4), sz = size(); ihs <= sz)
        {
            auto* payload = data() + ihs;
            auto payload_size = sz - ihs;
            auto frag_off = size_t(oxenc::big_to_host(hdr.frag_off) & 0x1Fff) * 8;

            auto ip_proto = static_cast<net::IPProtocol>(hdr.protocol);
            switch (ip_proto)
            {
                case net::IPProtocol::TCP:
                    if (frag_off <= TCP_CSUM_OFF && payload_size >= TCP_CSUM_OFF - frag_off + 2)
                    {
                        auto* tcp_hdr = reinterpret_cast<tcp_header*>(payload);
                        tcp_hdr->checksum =
                            utils::ipv4_tcp_checksum_diff(tcp_hdr->checksum, hdr.src, hdr.dest, src, dst);
                    }
                    break;
                case net::IPProtocol::UDP:
                case net::IPProtocol::UDP_LITE:  // UDP-Lite - same checksum place, same 0->0xFFff condition
                    if (frag_off <= UDP_CSUM_OFF && payload_size >= UDP_CSUM_OFF + 2)
                    {
                        auto* udp_hdr = reinterpret_cast<udp_header*>(payload);
                        udp_hdr->checksum =
                            utils::ipv4_udp_checksum_diff(udp_hdr->checksum, hdr.src, hdr.dest, src, dst);
                    }
                    break;
                case net::IPProtocol::DCCP:
                    if (frag_off <= UDP_CSUM_OFF || payload_size >= UDP_CSUM_OFF - frag_off + 2)
                    {
                        auto* tcp_hdr = reinterpret_cast<tcp_header*>(payload);
                        tcp_hdr->checksum =
                            utils::ipv4_tcp_checksum_diff(tcp_hdr->checksum, hdr.src, hdr.dest, src, dst);
                    }
                    break;
                default:
                    // do nothing (or not implemented)
                    break;
            }
        }

        hdr.checksum = utils::ipv4_checksum_diff(hdr.checksum, hdr.src, hdr.dest, src, dst);

        // set new IP addresses
        hdr.src = oxenc::host_to_big(src.addr);
        hdr.dest = oxenc::host_to_big(dst.addr);
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
    }

    std::optional<IPPacket> IPPacket::make_icmp_unreachable() const
    {
        if (is_ipv4())
        {
            const auto& header = *reinterpret_cast<const ip_header*>(data());
            auto ip_hdr_sz = header.header_len * 4;

            // ICMP unreachable includes a carbon copy of the illiciting packet prefix include *at
            // least* the header; we also include the first 8 bytes after the header:
            std::span orig_prefix{_buf.data(), std::min<size_t>(ip_hdr_sz + 8, _buf.size())};

            size_t pkt_size = sizeof(ip_header) + ICMP_HEADER_SIZE + orig_prefix.size();

            IPPacket pkt{pkt_size};

            auto& hdr = pkt.header();
            hdr.version = 0x04;
            hdr.header_len = 0x05;
            hdr.service_type = 0;
            hdr.checksum = 0;
            hdr.total_len = ntohs(pkt_size);
            hdr.src = header.dest;
            hdr.dest = header.src;
            hdr.protocol = 1;  // ICMP
            hdr.ttl = header.ttl;
            hdr.frag_off = oxenc::host_to_big<uint16_t>(0b01000000'00000000);

            std::byte* itr = pkt.data() + sizeof(ip_header);
            auto* icmp_begin = itr;
            *itr++ = std::byte{3};  // ICMP type 3 = 'destination unreachable'
            *itr++ = std::byte{7};  // ICMP code 7 = 'Destination host unknown error'

            // 2 byte checksum (we'll come back to this later)
            auto* checksum = reinterpret_cast<uint16_t*>(itr);
            itr += 2;
            // optional length byte + unused byte + optional 2-byte next hop MTU (for code 4, i.e.
            // not us).  We leave this all as zero (and buf is already 0 initialized).
            itr += (1 + 1 + 2);

            assert(itr == pkt.data() + sizeof(ip_header) + ICMP_HEADER_SIZE);

            // carbon copy the original packet prefix:
            std::memcpy(itr, orig_prefix.data(), orig_prefix.size());
            itr += orig_prefix.size();

            assert(itr == pkt.data() + pkt_size);

            // calculate checksum of ip header
            pkt.header().checksum = utils::ip_checksum(reinterpret_cast<const uint8_t*>(pkt.data()), sizeof(ip_header));

            // calculate icmp checksum from everything from icmp header (inclusive) to the end.
            *checksum =
                utils::ip_checksum(reinterpret_cast<const uint8_t*>(icmp_begin), std::distance(icmp_begin, itr));

            log::debug(logcat, "Constructed ICMP unreachable packet");
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

    std::string IPPacket::info_printer::to_string() const
    {
        if (pkt.is_ipv4())
        {
            return "IPv4[{}, {}B, src={}, dst={}]"_format(
                ip_protocol_name(pkt.protocol()), pkt.size(), *pkt.source_ipv4(), *pkt.dest_ipv4());
        }
        if (pkt.is_ipv6())
        {
            return "IPv6[{}, {}B, src={}, dst={}]"_format(
                ip_protocol_name(pkt.protocol()), pkt.size(), *pkt.source_ipv6(), *pkt.dest_ipv6());
        }
        return "IPPacket[<unknown-type>, {}B]"_format(pkt.size());
    }

}  // namespace llarp
