#include "policy.hpp"

#include "ip_packet.hpp"

#include <llarp/util/logging.hpp>
#include <llarp/util/str.hpp>

#include <stdexcept>

extern "C"
{
#include <netdb.h>
}

namespace llarp
{
    std::string to_string(protocol_flag p)
    {
        auto has_flag = [&p](protocol_flag f) { return (p & f) == f; };
        return "<{}{}{}{}>"_format(
            has_flag(protocol_flag::EXIT) ? "exit" : "host",
            has_flag(protocol_flag::QUIC_TUNNEL) ? "/quic" : "/no-quic",
            has_flag(protocol_flag::IPV4) ? "/ipv4" : "/no-ipv4",
            has_flag(protocol_flag::IPV6) ? "/ipv6" : "");
    }
    namespace net
    {
        static auto logcat = log::Cat("TrafficPolicy");

        // Two functions copied over from llarp/net/ip_packet_old.hpp
        static std::string ip_proto_str(IPProtocol proto)
        {
            if (const auto* ent = getprotobynumber(static_cast<int>(proto)))
                return ent->p_name;

            throw std::invalid_argument{"Cannot determine protocol name for IP Protocol: {}"_format(proto)};
        }

        bool ProtocolInfo::matches_packet_proto(const IPPacket& pkt) const { return pkt.protocol() == protocol; }

        bool ExitPolicy::allow_ip_traffic(const IPPacket& pkt) const
        {
            // ranges are always the allow list (if empty, we route nothing).  Add a 0.0.0.0/0 or
            // ::/0 if you want to allow everything.
            auto accept_range = pkt.is_ipv4()
                ? std::ranges::any_of(ranges, [dest = pkt.dest_ipv4()](const auto& r) { return r.contains(dest); })
                : std::ranges::any_of(ranges_v6, [dest = pkt.dest_ipv6()](const auto& r) { return r.contains(dest); });
            if (!accept_range)
                return false;

            // protocols only masks if non-empty:
            if (!protocols.empty())
                if (std::ranges::none_of(protocols, [&pkt](const auto& p) { return p.matches_packet_proto(pkt); }))
                    return false;

            return true;
        }

        void ProtocolInfo::bt_encode(oxenc::bt_list_producer&& btlp) const
        {
            btlp.append(static_cast<uint8_t>(protocol));
            if (port)
                btlp.append(*port);
        }

        ProtocolInfo ProtocolInfo::from_config(std::string_view config_input)
        {
            ProtocolInfo pi;
            auto parts = split(config_input, "/");
            if (parts.size() > 2)
                throw std::invalid_argument{"Unparseable IP protocol/port value '{}'"_format(config_input)};
            if (const auto* ent = ::getprotobyname(std::string{parts[0]}.c_str()))
                pi.protocol = static_cast<IPProtocol>(ent->p_proto);
            else if (uint8_t p; parts[0].starts_with("0x") && parse_int(parts[0], p, 16) && p > 0)
                pi.protocol = static_cast<IPProtocol>(p);
            else
                throw std::invalid_argument{"No such IP protocol '{}'"_format(parts[0])};

            if (parts.size() == 2)
                if (!parse_int(parts[1], pi.port.emplace()))
                    throw std::invalid_argument{"Invalid protocol port: '{}'"_format(parts[1])};
            return pi;
        }

        ProtocolInfo::ProtocolInfo(oxenc::bt_list_consumer&& enc)
        {
            protocol = IPProtocol{enc.consume_integer<uint8_t>()};
            if (not enc.is_finished())
                port = enc.consume_integer<uint16_t>();
        }

        void ExitPolicy::bt_decode(oxenc::bt_dict_consumer&& btdc)
        {
            try
            {
                if (auto protos = btdc.maybe<oxenc::bt_list_consumer>("p"))
                    while (not protos->is_finished())
                        protocols.emplace(protos->consume_list_consumer());

                if (auto rnges = btdc.maybe<oxenc::bt_list_consumer>("r"))
                {
                    while (not rnges->is_finished())
                    {
                        auto r = decode_ip_range(rnges->consume_string_view());
                        if (auto* r4 = std::get_if<ipv4_range>(&r))
                            ranges.push_back(std::move(*r4));
                        else
                            ranges_v6.push_back(std::get<ipv6_range>(r));
                    }
                }
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Failed to parse ExitPolicy: {}", e.what());
                throw;
            }
        }

        void ExitPolicy::bt_encode(oxenc::bt_dict_producer&& btdp) const
        {
            if (!protocols.empty())
            {
                auto protos = btdp.append_list("p");
                for (auto& p : protocols)
                    p.bt_encode(protos.append_list());
            }
            if (!ranges.empty() || !ranges_v6.empty())
            {
                auto rnges = btdp.append_list("r");
                for (const auto& r : ranges)
                    rnges.append(encode(r));
                for (const auto& r : ranges_v6)
                    rnges.append(encode(r));
            }
        }

        bool ExitPolicy::bt_decode(std::string_view buf)
        {
            try
            {
                bt_decode(oxenc::bt_dict_consumer{buf});
            }
            catch (const std::exception& e)
            {
                // DISCUSS: rethrow or print warning/return false...?
                auto err = "TrafficPolicy parsing exception: {}"_format(e.what());
                log::warning(logcat, "{}", err);
                throw std::runtime_error{err};
            }

            return true;
        }
    }  // namespace net
}  // namespace llarp
