#include "tun.hpp"

#include <variant>
#ifndef _WIN32
#include <sys/socket.h>
#endif

#include <llarp/auth/auth.hpp>
#include <llarp/constants/platform.hpp>
#include <llarp/contact/sns.hpp>
#include <llarp/dns/dns.hpp>
#include <llarp/dns/name.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/router/route_poker.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/logging/buffer.hpp>
#include <llarp/util/str.hpp>

namespace llarp::handlers
{
    static auto logcat = log::Cat("tun");

    bool TunEndpoint::maybe_hook_dns(
        const std::shared_ptr<dns::PacketSource>& source,
        const dns::Message& query,
        const quic::Address& to,
        const quic::Address& from)
    {
        if (not should_hook_dns_message(query))
            return false;

        auto job = std::make_shared<dns::QueryJob>(source, query, to, from);
        if (!handle_hooked_dns_message(query, [job](auto msg) { job->send_reply(msg.to_buffer()); }))
            job->cancel();
        return true;
    }

    /// Intercepts DNS IP packets on platforms where binding to a low port isn't viable.
    /// (windows/macos/ios/android ... aka everything that is not linux... funny that)
    class DnsInterceptor : public dns::PacketSource
    {
        ip_pkt_hook _hook;
        quic::Address _our_ip;  // maybe should be an IP type...?
        llarp::DnsConfig _config;

      public:
        explicit DnsInterceptor(ip_pkt_hook reply, quic::Address our_ip, llarp::DnsConfig conf)
            : _hook{std::move(reply)}, _our_ip{std::move(our_ip)}, _config{std::move(conf)}
        {}

        ~DnsInterceptor() override = default;

        void send_udp(
            const quic::Address& to, const quic::Address& from, std::span<const std::byte> payload) const override
        {
            log::critical(logcat, "DNS interceptor FIXME!");
            if (payload.empty())
                return;
            // FIXME: this
            (void)to;
            (void)from;
            (void)payload;
            // _hook(data.make_udp(to, from));
        }

        std::optional<quic::Address> bound_on() const override { return std::nullopt; }

        bool would_loop(const quic::Address& to, const quic::Address& from) const override
        {
            if constexpr (platform::is_apple)
            {
                // DNS on Apple is a bit weird because in order for the NetworkExtension itself to
                // send data through the tunnel we have to proxy DNS requests through Apple APIs
                // (and so our actual upstream DNS won't be set in our resolvers, which is why the
                // vanilla WouldLoop won't work for us).  However when active the mac also only
                // queries the main tunnel IP for DNS, so we consider anything else to be
                // upstream-bound DNS to let it through the tunnel.
                return to != _our_ip;
            }
            else if (auto maybe_addr = _config._query_bind)
            {
                const auto& addr = *maybe_addr;
                // omit traffic to and from our dns socket
                return addr == to or addr == from;
            }
            return false;
        }
    };

    class TunDNS : public dns::Server
    {
        const TunEndpoint* _tun;
        std::optional<quic::Address> _query_bind;
        quic::Address _our_ip;

      public:
        std::shared_ptr<dns::PacketSource> pkt_source;

        ~TunDNS() override = default;

        explicit TunDNS(TunEndpoint* ep, const llarp::DnsConfig& conf)
            : dns::Server{ep->router().loop(), conf, 0},
              _tun{ep},
              _query_bind{conf._query_bind},
              _our_ip{ep->get_ipv4()}  // FIXME: What about IPv6?
        {
            if (_query_bind)
                _our_ip.set_port(_query_bind->port());
        }

        std::shared_ptr<dns::PacketSource> make_packet_source_on(
            const quic::Address&, const llarp::DnsConfig& conf) override
        {
            (void)_tun;
            auto ptr = std::make_shared<DnsInterceptor>(
                [](IPPacket pkt) {
                    (void)pkt;
                    // ep->handle_write_ip_packet(pkt.ConstBuffer(), pkt.srcv6(), pkt.dstv6(), 0);
                },
                _our_ip,
                conf);
            pkt_source = ptr;
            return ptr;
        }
    };

    // NB: It looks like this could/should be called during the constructor,
    // but as it passes weak_from_this to the dns server, it has to be after.
    void TunEndpoint::setup_dns()
    {
        log::debug(logcat, "{} setting up DNS...", name());

        auto& dns_config = _router.config().dns;
        const auto& info = get_vpn_interface()->interface_info();

        if (dns_config.l3_intercept)
        {
            _dns = std::make_unique<TunDNS>(this, dns_config);
            auto* dns = static_cast<TunDNS*>(_dns.get());

            uint16_t p = 53;

            while (p < 100)
            {
                try
                {
                    _packet_router->add_udp_handler(p, [this, dns](IPPacket pkt) {
                        if (dns->maybe_handle_payload(dns->pkt_source, pkt.destination(), pkt.source(), pkt.udp_data()))
                            return;

                        handle_outbound_packet(std::move(pkt));
                    });
                }
                catch (const std::exception& e)
                {
                    if (p += 1; p >= 100)
                        throw std::runtime_error{"Failed to port map udp handler: {}"_format(e.what())};
                }
            }
        }
        else
            _dns = std::make_unique<dns::Server>(_router.loop(), dns_config, info.index);

        _dns->add_resolver(weak_from_this());
        _dns->start();

        if (dns_config.l3_intercept)
        {
            if (auto vpn = _router.vpn_platform())
            {
                // get the first local address we know of
                std::optional<quic::Address> localaddr;

                for (auto res : _dns->get_all_resolvers())
                {
                    if (auto ptr = res.lock())
                    {
                        localaddr = ptr->get_local_addr();

                        if (localaddr)
                            break;
                    }
                }
                if (platform::is_windows)
                {
                    // auto dns_io = vpn->create_packet_io(0, localaddr);
                    // router().loop()->add_ticker([dns_io, handler = m_PacketRouter]() {
                    //   net::IPPacket pkt = dns_io->ReadNextPacket();
                    //   while (not pkt.empty())
                    //   {
                    //     handler->HandleIPPacket(std::move(pkt));
                    //     pkt = dns_io->ReadNextPacket();
                    //   }
                    // });
                    // m_RawDNS = dns_io;
                }

                (void)vpn;
            }

            if (_raw_DNS)
                _raw_DNS->Start();
        }
    }

    nlohmann::json TunEndpoint::ExtractStatus() const
    {
        // auto obj = service::Endpoint::ExtractStatus();
        // obj["ifaddr"] = m_OurRange.to_string();
        // obj["ifname"] = m_IfName;

        // std::vector<std::string> upstreamRes;
        // for (const auto& ent : m_DnsConfig.upstream_dns)
        //   upstreamRes.emplace_back(ent.to_string());
        // obj["ustreamResolvers"] = upstreamRes;

        // std::vector<std::string> localRes;
        // for (const auto& ent : m_DnsConfig.bind_addr)
        //   localRes.emplace_back(ent.to_string());
        // obj["localResolvers"] = localRes;

        // // for backwards compat
        // if (not m_DnsConfig.bind_addr.empty())
        //   obj["localResolver"] = localRes[0];

        // nlohmann::json ips{};
        // for (const auto& item : m_IPActivity)
        // {
        //   nlohmann::json ipObj{{"lastActive", to_json(item.second)}};
        //   std::string remoteStr;
        //   AlignedBuffer<32> addr = m_IPToAddr.at(item.first);
        //   if (m_SNodes.at(addr))
        //     remoteStr = RouterID(addr.as_array()).to_string();
        //   else
        //     remoteStr = service::Address(addr.as_array()).to_string();
        //   ipObj["remote"] = remoteStr;
        //   std::string ipaddr = item.first.to_string();
        //   ips[ipaddr] = ipObj;
        // }
        // obj["addrs"] = ips;
        // obj["ourIP"] = m_OurIP.to_string();
        // obj["nextIP"] = m_NextIP.to_string();
        // obj["maxIP"] = m_MaxIP.to_string();
        // return obj;
        return {};
    }

    void TunEndpoint::reconfigure_dns(std::vector<quic::Address> servers)
    {
        if (_dns)
        {
            for (auto weak : _dns->get_all_resolvers())
            {
                if (auto ptr = weak.lock())
                    ptr->reset_resolver(servers);
            }
        }
    }

    TunEndpoint::TunEndpoint(Router& r) : _router{r}
    {
        _packet_router =
            std::make_shared<vpn::PacketRouter>([this](IPPacket pkt) { handle_outbound_packet(std::move(pkt)); });

        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        auto& net_conf = _router.config().network;

        _exit_policy = net_conf.traffic_policy;

        if (net_conf.path_alignment_timeout)
        {
            if (is_service_node())
                throw std::runtime_error{"Service nodes cannot specify path alignment timeout!"};

            _path_alignment_timeout = *net_conf.path_alignment_timeout;
        }

        ipv6_enabled = net_conf.enable_ipv6;
        if (ipv6_enabled)
        {
            _local_ipv6_net = net_conf._local_ipv6_net;
            if (_local_ipv6_net)
                _local_ipv6_range_iterator.emplace(*_local_ipv6_net);
        }

        _if_name = net_conf._if_name.value_or("");
        if (net_conf._local_ip_net)
            _local_net = *net_conf._local_ip_net;

        if (net_conf.addr_map_persist_file)
        {
            _persisting_addr_file = net_conf.addr_map_persist_file;
            persist_addrs = true;
        }

        for (auto& [remote, local] : net_conf._reserved_local_ipv4)
            _local_ipv4_mapping.insert_or_assign(local, remote);
        for (auto& [remote, local] : net_conf._reserved_local_ipv6)
            _local_ipv6_mapping.insert_or_assign(local, remote);

        log::debug(logcat, "Tun constructing IPRange iterator on local network: {}", _local_net);
        _local_range_iterator = IPRangeIterator{_local_net};

        _local_netaddr = NetworkAddress{_router.local_rid(), not _router.is_service_node()};
        _local_ipv4_mapping.insert_or_assign(_local_net.ip, std::move(_local_netaddr));

        vpn::InterfaceInfo info;
        info.ifname = _if_name;
        info.addrs.emplace_back(_local_net);

        if (ipv6_enabled and _local_ipv6_net)
        {
            log::info(logcat, "{} using ipv6 range:{}", name(), *_local_ipv6_net);
            info.addrs.emplace_back(*_local_ipv6_net);
        }

        log::debug(logcat, "{} setting up network...", name());

        log::info(logcat, "{} using IPv4 address range {}", name(), _local_net);
        if (ipv6_enabled && _local_ipv6_net)
            log::info(logcat, "{} using IPv6 address range {}", name(), _local_ipv6_net);

        _net_if = router().vpn_platform()->create_interface(std::move(info), &_router);
        _if_name = _net_if->interface_info().ifname;

        log::info(logcat, "{} got network interface:{}", name(), _if_name);

        auto pkt_hook = [this]() mutable {
            for (auto pkt = _net_if->read_next_packet(); not pkt.empty(); pkt = _net_if->read_next_packet())
            {
                log::trace(logcat, "packet router receiving {}", pkt.info_line());
                _packet_router->handle_ip_packet(std::move(pkt));
            }
        };

#ifdef __linux__
        _poller =
            std::make_unique<LinuxPoller>(_net_if->PollFD(), _router.loop()->get_event_base(), std::move(pkt_hook));
#endif
        if (not _poller)
        {
            auto err = "{} failed to add network interface!"_format(name());
            log::critical(logcat, "{}", err);
            throw std::runtime_error{std::move(err)};
        }
    }

    static bool is_random_snode(const dns::Message& msg) { return msg.questions[0].IsName("random.snode"); }

    static bool is_localhost_loki(const dns::Message& msg) { return msg.questions[0].IsLocalhost(); }

    static dns::Message& clear_dns_message(dns::Message& msg)
    {
        msg.authorities.resize(0);
        msg.additional.resize(0);
        msg.answers.resize(0);
        msg.hdr_fields &= ~dns::flags_RCODENameError;
        return msg;
    }

    template <typename T, typename... Args>
    static std::optional<T> try_making(Args&&... args)
    {
        try
        {
            return std::make_optional<T>(std::forward<Args>(args)...);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    bool TunEndpoint::handle_hooked_dns_message(dns::Message msg, std::function<void(dns::Message)> reply)
    {
        log::trace(logcat, "handle_hooked_dns_message");
        (void)msg;
        (void)reply;
        // auto ReplyToSNodeDNSWhenReady = [this, reply](RouterID snode, auto msg, bool isV6) ->
        // bool {
        //   return EnsurePathToSNode(
        //       snode,
        //       [this, snode, msg, reply, isV6](
        //           const RouterID&,
        //           std::shared_ptr<session::BaseSession> s,
        //           [[maybe_unused]] session_tag tag) {
        //         SendDNSReply(snode, s, msg, reply, isV6);
        //       });
        // };
        // auto ReplyToLokiDNSWhenReady = [this, reply, timeout = PathAlignmentTimeout()](
        //                                    service::Address addr, auto msg, bool isV6) -> bool {
        //   using service::Address;
        //   using service::OutboundContext;
        //   if (HasInboundConvo(addr))
        //   {
        //     // if we have an inbound convo to this address don't mark as outbound so we don't
        //     have a
        //     // state race this codepath is hit when an application verifies that reverse and
        //     forward
        //     // dns records match for an inbound session
        //     SendDNSReply(addr, this, msg, reply, isV6);
        //     return true;
        //   }
        //   MarkAddressOutbound(addr);
        //   return EnsurePathToService(
        //       addr,
        //       [this, addr, msg, reply, isV6](const Address&, OutboundContext* ctx) {
        //         SendDNSReply(addr, ctx, msg, reply, isV6);
        //       },
        //       timeout);
        // };

        // auto ReplyToDNSWhenReady = [ReplyToLokiDNSWhenReady, ReplyToSNodeDNSWhenReady](
        //                                std::string name, auto msg, bool isV6) {
        //   if (auto saddr = service::Address(); saddr.FromString(name))
        //     ReplyToLokiDNSWhenReady(saddr, msg, isV6);

        //   if (auto rid = RouterID(); rid.from_snode_address(name))
        //     ReplyToSNodeDNSWhenReady(rid, msg, isV6);
        // };

        // auto ReplyToLokiSRVWhenReady = [this, reply, timeout = PathAlignmentTimeout()](
        //                                    service::Address addr, auto msg) -> bool {
        //   using service::Address;
        //   using service::OutboundContext;
        //   // TODO: how do we handle SRV record lookups for inbound sessions?
        //   MarkAddressOutbound(addr);
        //   return EnsurePathToService(
        //       addr,
        //       [msg, addr, reply](const Address&, OutboundContext* ctx) {
        //         if (ctx == nullptr)
        //           return;

        //         const auto& introset = ctx->GetCurrentIntroSet();
        //         msg->AddSRVReply(introset.GetMatchingSRVRecords(addr.subdomain));
        //         reply(*msg);
        //       },
        //       timeout);
        // };

        /*
        if (msg.answers.size() > 0)
        {
          const auto& answer = msg.answers[0];
          if (answer.HasCNameForTLD(".snode"))
          {
            llarp_buffer_t buf(answer.rData);
            auto qname = dns::DecodeName(&buf, true);
            if (not qname)
              return false;
            RouterID addr;
            if (not addr.from_snode_address(*qname))
              return false;
            auto replyMsg = std::make_shared<dns::Message>(clear_dns_message(msg));
            return ReplyToSNodeDNSWhenReady(addr, std::move(replyMsg), false);
          }
          else if (answer.HasCNameForTLD(".loki"))
          {
            llarp_buffer_t buf(answer.rData);
            auto qname = dns::DecodeName(&buf, true);
            if (not qname)
              return false;

            service::Address addr;
            if (not addr.FromString(*qname))
              return false;

            auto replyMsg = std::make_shared<dns::Message>(clear_dns_message(msg));
            return ReplyToLokiDNSWhenReady(addr, replyMsg, false);
          }
        }
        */

        if (msg.questions.size() != 1)
        {
            log::debug(logcat, "bad number of dns questions: {}", msg.questions.size());
            return false;
        }

        std::string our_name = _router.local_rid().to_network_address(_router.is_service_node());

        std::string qname = msg.questions[0].Name();
        const auto nameparts = split(qname, ".");
        std::string hostname, tld;
        if (nameparts.size() >= 2)
        {
            hostname = nameparts[nameparts.size() - 2];
            tld = nameparts[nameparts.size() - 1];
        }
        else
        {
            log::debug(logcat, "bad DNS request, no TLD or hostname: {}", qname);
            return false;
        }
        bool is_localhost = hostname == "localhost"s && tld == "loki"s;
        std::string sns_name;
        if (nameparts.size() >= 2 and ends_with(qname, ".loki"))
        {
            sns_name = hostname;
            sns_name += ".loki"sv;
        }
        /*
        if (msg.questions[0].qtype == dns::qTypeTXT)
        {
          RouterID snode;
          if (snode.from_snode_address(qname))
          {
            if (auto rc = router().node_db().get_rc(snode))
              msg.AddTXTReply(std::string{rc->view()});
            else
              msg.AddNXReply();
            reply(msg);

            return true;
          }

          if (msg.questions[0].IsLocalhost() and msg.questions[0].HasSubdomains())
          {
            const auto subdomain = msg.questions[0].Subdomains();
            if (subdomain == "exit")
            {
              if (HasExit())
              {
                std::string s;
                _exit_map.ForEachEntry([&s](const auto& range, const auto& exit) {
                  fmt::format_to(std::back_inserter(s), "{}={}; ", range, exit);
                });
                msg.AddTXTReply(std::move(s));
              }
              else
              {
                msg.AddNXReply();
              }
            }
            else if (subdomain == "netid")
            {
              msg.AddTXTReply(fmt::format("netid={};", RelayContact::ACTIVE_NETID));
            }
            else
            {
              msg.AddNXReply();
            }
          }
          else
          {
            msg.AddNXReply();
          }

          reply(msg);
        }
        else if (msg.questions[0].qtype == dns::qTypeMX)
        {
          // mx record
          service::Address addr;
          if (addr.FromString(qname, ".loki") || addr.FromString(qname, ".snode")
              || is_random_snode(msg) || is_localhost_loki(msg))
          {
            msg.AddMXReply(qname, 1);
          }
          else if (service::is_valid_name(sns_name))
          {
            lookup_name(
                sns_name, [msg, sns_name, reply](std::string name_result, bool success) mutable {
                  if (success)
                  {
                    msg.AddMXReply(name_result, 1);
                  }
                  else
                    msg.AddNXReply();

                  reply(msg);
                });

            return true;
          }
          else
            msg.AddNXReply();
          reply(msg);
        }
        else if (msg.questions[0].qtype == dns::qTypeCNAME)
        {
          if (is_random_snode(msg))
          {
            if (auto random = router().GetRandomGoodRouter())
            {
              msg.AddCNAMEReply(random->to_string(), 1);
            }
            else
              msg.AddNXReply();
          }
          else if (msg.questions[0].IsLocalhost() and msg.questions[0].HasSubdomains())
          {
            const auto subdomain = msg.questions[0].Subdomains();
            if (subdomain == "exit" and HasExit())
            {
              _exit_map.ForEachEntry(
                  [&msg](const auto&, const auto& exit) { msg.AddCNAMEReply(exit.to_string(), 1);
                  });
            }
            else
            {
              msg.AddNXReply();
            }
          }
          else if (is_localhost_loki(msg))
          {
            size_t counter = 0;
            context->ForEachService(
                [&](const std::string&, const std::shared_ptr<service::Endpoint>& service) ->
                bool {
                  const service::Address addr = service->GetIdentity().pub.Addr();
                  msg.AddCNAMEReply(addr.to_string(), 1);
                  ++counter;
                  return true;
                });
            if (counter == 0)
              msg.AddNXReply();
          }
          else
            msg.AddNXReply();
          reply(msg);
        }
        */
        /*else*/ if (msg.questions[0].qtype == dns::qTypeA || msg.questions[0].qtype == dns::qTypeAAAA)
        {
            auto reply_with_mapped_address = [reply, msg](const std::optional<ipv4>& maybe_ip) mutable {
                if (maybe_ip)
                {
                    msg.add_IN_reply(maybe_ip->addr);
                    reply(msg);
                    return;
                }
                msg.add_nx_reply();
                reply(msg);
            };

            const bool isV6 = msg.questions[0].qtype == dns::qTypeAAAA;
            const bool isV4 = msg.questions[0].qtype == dns::qTypeA;
            (void)isV6;
            (void)isV4;
            /*
            if (isV6 && !ipv6_enabled)
            {  // empty reply but not a NXDOMAIN so that client can retry IPv4
              msg.AddNSReply("localhost.loki.");
            }
            // on MacOS this is a typeA query
            else if (is_random_snode(msg))
            {
              if (auto random = router().GetRandomGoodRouter())
              {
                msg.AddCNAMEReply(random->to_string(), 1);
                return ReplyToSNodeDNSWhenReady(*random, std::make_shared<dns::Message>(msg),
                isV6);
              }

              msg.AddNXReply();
            }
            */
            /*else*/ if (is_localhost)
            {
                // FIXME: the code below checks about if we have a tun bound, and
                // if we're operating as an exit (if that was requested), and those
                // concepts need to be revived
                /*
                const bool lookingForExit = msg.questions[0].Subdomains() == "exit";
                huint128_t ip = GetIfAddr();
                if (ip.h)
                {
                  if (lookingForExit)
                  {
                    if (HasExit())
                    {
                      _exit_map.ForEachEntry(
                          [&msg](const auto&, const auto& exit) { msg.AddCNAMEReply(exit.to_string());
                          });
                      msg.AddINReply(ip, isV6);
                    }
                    else
                    {
                      msg.AddNXReply();
                    }
                  }
                  else
                  {
                    msg.AddCNAMEReply(our_name, 1);
                    msg.AddINReply(ip, isV6);
                  }
                }
                else
                {
                  msg.AddNXReply();
                }
                */

                msg.add_CNAME_reply(our_name, 1);
                msg.add_IN_reply(_local_net.ip.addr);
                reply(msg);
            }
            else if (auto maybe_netaddr = try_making<NetworkAddress>("{}.{}"_format(hostname, tld)))
            {
                // TODO: handle .snode (if we want, or explicitly don't allow)

                reply_with_mapped_address(map_session_to_local_ip(*maybe_netaddr));
                return true;
            }
            else if (tld == "loki"sv)
            {
                _router.session_endpoint().resolve_sns(
                    "{}.loki"_format(hostname),
                    [this, reply, reply_with_mapped_address, msg](std::optional<NetworkAddress> maybe_netaddr) mutable {
                        if (maybe_netaddr)
                        {
                            reply_with_mapped_address(map_session_to_local_ip(*maybe_netaddr));
                            return;
                        }
                        msg.add_nx_reply();
                        reply(msg);
                    });
            }
            /*
            else if (addr.FromString(qname, ".loki"))
            {
              if (isV4 && ipv6_enabled)
              {
                msg.hdr_fields |= dns::flags_QR | dns::flags_AA | dns::flags_RA;
              }
              else
              {
                return ReplyToLokiDNSWhenReady(addr, std::make_shared<dns::Message>(msg), isV6);
              }
            }
            else if (addr.FromString(qname, ".snode"))
            {
              if (isV4 && ipv6_enabled)
              {
                msg.hdr_fields |= dns::flags_QR | dns::flags_AA | dns::flags_RA;
              }
              else
              {
                return ReplyToSNodeDNSWhenReady(
                    addr.as_array(), std::make_shared<dns::Message>(msg), isV6);
              }
            }
            else if (service::is_valid_name(sns_name))
            {
              lookup_name(
                  sns_name,
                  [msg = std::make_shared<dns::Message>(msg),
                   name = Name(),
                   sns_name,
                   isV6,
                   reply,
                   ReplyToDNSWhenReady](std::string name_result, bool success) mutable {
                    if (not success)
                    {
                      log::warning(logcat, "{} (ONS name: {}) not resolved", name, sns_name);
                      msg->AddNXReply();
                      reply(*msg);
                    }

                    ReplyToDNSWhenReady(name_result, msg, isV6);
                  });
              return true;
            }
            else
              msg.AddNXReply();

            reply(msg);
          }
          else if (msg.questions[0].qtype == dns::qTypePTR)
          {
            // reverse dns
            if (auto ip = dns::DecodePTR(msg.questions[0].qname))
            {
              if (auto maybe = ObtainAddrForIP(*ip))
              {
                var::visit([&msg](auto&& result) { msg.AddAReply(result.to_string()); }, *maybe);
                reply(msg);
                return true;
              }
            }

            msg.AddNXReply();
            reply(msg);
            return true;
          }
          else if (msg.questions[0].qtype == dns::qTypeSRV)
          {
            auto srv_for = msg.questions[0].Subdomains();
            auto name = msg.questions[0].qname;
            if (is_localhost_loki(msg))
            {
              msg.AddSRVReply(intro_set().GetMatchingSRVRecords(srv_for));
              reply(msg);
              return true;
            }
            LookupServiceAsync(
                name,
                srv_for,
                [reply, msg = std::make_shared<dns::Message>(std::move(msg))](auto records) {
                  if (records.empty())
                  {
                    msg->AddNXReply();
                  }
                  else
                  {
                    msg->AddSRVReply(records);
                  }
                  reply(*msg);
                });
            return true;
          }
          else
          {
            msg.AddNXReply();
            reply(msg);
          }
          return true;
          */
        }
        return true;
    }

    bool TunEndpoint::supports_ipv6() const { return ipv6_enabled; }

    // FIXME: pass in which question it should be addressing
    bool TunEndpoint::should_hook_dns_message(const dns::Message& msg) const
    {
        // llarp::service::Address addr;
        if (msg.questions.size() == 1)
        {
            /// hook every .loki
            if (msg.questions[0].HasTLD(".loki"))
                return true;
            /// hook every .snode
            if (msg.questions[0].HasTLD(".snode"))
                return true;
            // hook any ranges we own
            if (msg.questions[0].qtype == llarp::dns::qTypePTR)
            {
                if (auto ip = dns::DecodePTR(msg.questions[0].qname))
                {
                    if (auto* v4 = std::get_if<ipv4>(&*ip))
                        return _local_net.contains(*v4);
                    if (_local_ipv6_net)
                        return _local_ipv6_net->contains(std::get<ipv6>(*ip));
                }
                return false;
            }
        }
        for (const auto& answer : msg.answers)
        {
            if (answer.HasCNameForTLD(".loki"))
                return true;
            if (answer.HasCNameForTLD(".snode"))
                return true;
        }
        return false;
    }

    std::string TunEndpoint::get_if_name() const { return _if_name; }

    const ipv4& TunEndpoint::get_ipv4() const { return _local_net.ip; }
    const ipv6* TunEndpoint::get_ipv6() const { return _local_ipv6_net ? &_local_ipv6_net->ip : nullptr; }

    bool TunEndpoint::is_service_node() const { return _router.is_service_node(); }

    bool TunEndpoint::is_exit_node() const { return _router.is_exit_node(); }

    bool TunEndpoint::stop()
    {
        // stop vpn tunnel
        if (_net_if)
            _net_if->Stop();
        if (_raw_DNS)
            _raw_DNS->Stop();

        // save address map if applicable
        if (_persisting_addr_file and not platform::is_android)
        {
            const auto& file = *_persisting_addr_file;
            log::debug(logcat, "{} saving address map to {}", name(), file);
            // if (auto maybe = util::OpenFileStream<fs::ofstream>(file, std::ios_base::binary))
            // {
            //   std::map<std::string, std::string> addrmap;
            //   for (const auto& [ip, addr] : m_IPToAddr)
            //   {
            //     if (not m_SNodes.at(addr))
            //     {
            //       const service::Address a{addr.as_array()};
            //       if (HasInboundConvo(a))
            //         addrmap[ip.to_string()] = a.to_string();
            //     }
            //   }
            //   const auto data = oxenc::bt_serialize(addrmap);
            //   maybe->write(data.data(), data.size());
            // }
        }

        if (_dns)
            _dns->stop();

        return true;
    }

    template <typename RangeIterator>
    static std::optional<typename RangeIterator::ip_t> get_next_local_ipvX(
        RangeIterator& rit,
        const typename RangeIterator::ip_net_t& local_net,
        address_map<typename RangeIterator::ip_t>& local_mapping)
    {
        // if our IP range is exhausted, we loop back around to see if any have been unmapped from terminated
        // sessions; we only want to reset the iterator and loop back through once though
        bool has_reset = false;

        do
        {
            // this will be std::nullopt if IP range is exhausted OR the IP incrementing overflowed (basically
            // equal)
            if (auto maybe_next_ip = rit.next_ip())
            {
                if (not local_mapping.has_local(*maybe_next_ip))
                    return maybe_next_ip;
                // local IP is already assigned; try again
                continue;
            }

            if (has_reset)
                break;

            log::debug(logcat, "Resetting IP range iterator for range: {}...", local_net);
            rit.reset();
            has_reset = true;
        } while (true);

        return std::nullopt;
    }

    std::optional<ipv4> TunEndpoint::get_next_local_ipv4()
    {
        return get_next_local_ipvX(_local_range_iterator, _local_net, _local_ipv4_mapping);
    }

    std::optional<ipv6> TunEndpoint::get_next_local_ipv6()
    {
        if (!_local_ipv6_range_iterator || !_local_ipv6_net)
            return std::nullopt;

        return get_next_local_ipvX(*_local_ipv6_range_iterator, *_local_ipv6_net, _local_ipv6_mapping);
    }

    std::optional<ipv4> TunEndpoint::map_session_to_local_ip(const NetworkAddress& remote)
    {
        std::optional<ipv4> ret = std::nullopt;

        // first: check if we have a config value for this remote
        if (auto maybe_ip = _local_ipv4_mapping.get_local(remote))
        {
            ret = maybe_ip;
            log::debug(logcat, "Local IP for session to remote ({}) pre-loaded from config: {}", remote, *maybe_ip);
        }
        // Otherwise go find a new local IP for it
        else if (auto maybe_next_ip = get_next_local_ipv4())
        {
            log::debug(logcat, "Local IP for session to remote ({}) assigned: {}", remote, *maybe_next_ip);
            ret = maybe_next_ip;
            _local_ipv4_mapping.insert_or_assign(*maybe_next_ip, remote);
        }
        else
            log::critical(logcat, "TUN device failed to assign local private IP for session to remote: {}", remote);

        return ret;
    }

    void TunEndpoint::unmap_session_to_local_ip(const NetworkAddress& remote)
    {
        if (_local_ipv4_mapping.has_remote(remote))
        {
            _local_ipv4_mapping.unmap(remote);
            log::debug(logcat, "TUN device unmapped session to remote: {}", remote);
        }
        else
            log::warning(logcat, "TUN device could not unmap session (remote: {})", remote);
    }

    // handles an outbound packet going OUT from user -> network
    void TunEndpoint::handle_outbound_packet(IPPacket pkt)
    {
        ipv4 src, dest;
        if (pkt.is_ipv6())
        {
            log::debug(logcat, "Dropping IPv6 packet: not yet supported");
            return;
        }
        if (!pkt.is_ip())
        {
            log::debug(logcat, "Dropping non-IP packet");
            return;
        }

        log::trace(logcat, "outbound packet: {}: {}", pkt.info_line(), buffer_printer{pkt.span()});

        src = pkt.source_ipv4();
        dest = pkt.dest_ipv4();

        log::trace(logcat, "src:{}, dest:{}", src, dest);

        if constexpr (llarp::platform::is_apple)
        {
            if (dest == _local_net.ip)
            {
                rewrite_and_send_packet(std::move(pkt), std::move(src), std::move(dest));
                return;
            }
        }

        // we pass `dest` because that is our local private IP on the outgoing IPPacket
        if (auto maybe_remote = _local_ipv4_mapping.get_remote(dest))
        {
            auto& remote = *maybe_remote;
            pkt.clear_addresses();

            if (auto session = _router.session_endpoint().get_session(remote))
            {
                log::trace(
                    logcat,
                    "Dispatching outbound {}B packet for session (remote: {}): {}",
                    pkt.size(),
                    remote,
                    pkt.info_line());
                session->send_path_data_message(pkt.span());
            }
            else
            {
                if (_router.session_endpoint().have_pending_session(remote))
                {
                    _router.session_endpoint().queue_session_packet(remote, std::move(pkt));
                    return;
                }

                log::debug(logcat, "No session for remote: {} for outbound packet, attempting to create one!", remote);

                if (remote.is_client())
                {
                    _router.session_endpoint().lookup_client_intro(
                        remote.router_id(),
                        [this, remote, pkt = std::move(pkt)](std::optional<llarp::ClientContact> cc) mutable {
                            if (cc)
                            {
                                log::debug(logcat, "client intro for {} found:\n{}", remote, *cc);
                                _router.session_endpoint().initiate_remote_session(remote, nullptr);
                                _router.session_endpoint().queue_session_packet(remote, std::move(pkt));
                                return;
                            }
                            log::debug(logcat, "It appears {} has no contact information available.", remote);
                            if (auto icmp = pkt.make_icmp_unreachable())
                                send_packet_to_net_if(std::move(*icmp));
                        });
                }
                else
                {
                    _router.session_endpoint().lookup_relay_contact(
                        remote.router_id(),
                        [this, remote, pkt = std::move(pkt)](std::optional<llarp::RemoteRC> rc) mutable {
                            if (rc)
                            {
                                log::debug(logcat, "Relay contact for {} found:\n{}", remote, *rc);
                                _router.session_endpoint().initiate_remote_session(remote, nullptr);
                                _router.session_endpoint().queue_session_packet(remote, std::move(pkt));
                                return;
                            }
                            log::debug(logcat, "It appears {} has no contact information available.", remote);
                            if (auto icmp = pkt.make_icmp_unreachable())
                                send_packet_to_net_if(std::move(*icmp));
                        });
                }
            }
        }
        else
        {
            log::trace(logcat, "Could not find remote for route {}", pkt.info_line());

            // make ICMP unreachable
            if (auto icmp = pkt.make_icmp_unreachable())
                send_packet_to_net_if(std::move(*icmp));
        }
    }

    std::optional<ipv4> TunEndpoint::obtain_src_for_ipv4_remote(const NetworkAddress& remote)
    {
        if (auto maybe_src = _local_ipv4_mapping.get_local(remote))
            return maybe_src;

        log::warning(logcat, "Unable to find mapped IPv4 for inbound packet from remote {}", remote);
        return std::nullopt;
    }
    std::optional<ipv6> TunEndpoint::obtain_src_for_ipv6_remote(const NetworkAddress& remote)
    {
        if (auto maybe_src = _local_ipv6_mapping.get_local(remote))
            return maybe_src;

        log::warning(logcat, "Unable to find mapped IPv6 for inbound packet from remote {}", remote);
        return std::nullopt;
    }

    void TunEndpoint::send_packet_to_net_if(IPPacket pkt)
    {
        _router.loop()->call([this, pkt = std::move(pkt)]() mutable { _net_if->write_packet(std::move(pkt)); });
    }

    void TunEndpoint::rewrite_and_send_packet(IPPacket&& pkt, const ipv4& src, const ipv4& dest)
    {
        pkt.update_ipv4_address(src, dest);
        send_packet_to_net_if(std::move(pkt));
    }
    void TunEndpoint::rewrite_and_send_packet(IPPacket&& pkt, const ipv6& src, const ipv6& dest)
    {
        pkt.update_ipv6_address(src, dest);
        send_packet_to_net_if(std::move(pkt));
    }

    // FIXME: replace session_tag with packet type flag (uint8_t), because session_tag is definitely
    // not the right thing.
    // FIXME 2: we need separate flags for to-exit and from-exit
    void TunEndpoint::handle_inbound_packet(IPPacket pkt, session_tag tag, NetworkAddress remote)
    {
        bool to_exit = false;    // TODO FIXME
        bool from_exit = false;  // TODO FIXME

        if (to_exit)  // traffic exiting through this node
        {
            log::trace(logcat, "inbound exit pkt for exit node: {}", pkt.info_line());
            if (not is_allowing_traffic(pkt))
            {
                log::warning(logcat, "Dropping inbound exit packet: denied by local traffic policy");
                return;
            }

            if (pkt.is_ipv4())
            {
                if (auto src = obtain_src_for_ipv4_remote(remote))
                    return rewrite_and_send_packet(std::move(pkt), *src, pkt.dest_ipv4());
            }
            else
            {
                if (auto src = obtain_src_for_ipv6_remote(remote))
                    return rewrite_and_send_packet(std::move(pkt), *src, pkt.dest_ipv6());
            }
            return;
        }

        if (from_exit)  // return traffic coming back from an exit
        {
            log::trace(logcat, "inbound return exit pkt: {}", pkt.info_line());
            if (pkt.is_ipv4())
                return rewrite_and_send_packet(std::move(pkt), pkt.source_ipv4(), _local_net.ip);
            if (_local_ipv6_net)
                return rewrite_and_send_packet(std::move(pkt), pkt.source_ipv6(), _local_ipv6_net->ip);
            return;
        }

        log::trace(logcat, "inbound pkt to host: {}", pkt.info_line());
        if (pkt.is_ipv4())
        {
            if (auto src = obtain_src_for_ipv4_remote(remote))
                return rewrite_and_send_packet(std::move(pkt), *src, _local_net.ip);
        }
        else if (_local_ipv6_net)
        {
            if (auto src = obtain_src_for_ipv6_remote(remote))
                return rewrite_and_send_packet(std::move(pkt), *src, _local_ipv6_net->ip);
        }
    }

    void TunEndpoint::start_poller()
    {
        if (not _poller->start())
            throw std::runtime_error{"TUN failed to start FD poller!"};
        log::trace(logcat, "TUN successfully started FD poller!");
    }

    bool TunEndpoint::is_allowing_traffic(const IPPacket& pkt) const
    {
        return _exit_policy ? _exit_policy->allow_ip_traffic(pkt) : true;
    }

    std::pair<std::optional<ipv4>, std::optional<ipv6>> TunEndpoint::get_mapped_ip(const NetworkAddress& addr)
    {
        return {_local_ipv4_mapping.get_local(addr), _local_ipv6_mapping.get_local(addr)};
    }

    TunEndpoint::~TunEndpoint()
    {
        log::trace(logcat, "TunEndpoint::~TunEndpoint()");
    }

}  // namespace llarp::handlers
