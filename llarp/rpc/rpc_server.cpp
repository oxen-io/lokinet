#include "rpc_server.hpp"

#include "rpc_request.hpp"

#include <llarp/config/config.hpp>
#include <llarp/config/ini.hpp>
#include <llarp/constants/version.hpp>
#include <llarp/contact/client_contact.hpp>
#ifndef LOKINET_LIBRARY_ONLY
#include <llarp/dns/dns.hpp>
#include <llarp/dns/server.hpp>
#endif
#include <llarp/messages/common.hpp>
#include <llarp/router/router.hpp>
#include <llarp/rpc/rpc_request_definitions.hpp>
#include <llarp/util/logging/buffer.hpp>

#include <nlohmann/json.hpp>
#include <oxenc/base32z.h>

#include <exception>
#include <vector>

namespace llarp::rpc
{
    static auto logcat = llarp::log::Cat("rpc-server");

    template <typename T>
        requires std::derived_from<T, RPCRequest>
    static void log_print_rpc(T& req)
    {
        log::info(logcat, "RPC Server received request for endpoint `{}`", req.name);
    }

#ifndef LOKINET_LIBRARY_ONLY
    // Fake packet source that serializes repsonses back into dns
    class DummyPacketSource final : public dns::PacketSource
    {
        std::function<void(std::optional<dns::Message>)> func;

      public:
        explicit DummyPacketSource(std::function<void(std::optional<dns::Message>)> func) : func{std::move(func)} {}

        bool would_loop(const quic::Address&, const quic::Address&) const override { return false; };

        /// send packet with src and dst address containing buf on this packet source
        void send_udp(const quic::Address&, const quic::Address&, std::span<const std::byte> payload) const override
        {
            func(dns::maybe_parse_dns_msg(payload));
        }

        /// returns the sockaddr we are bound on if applicable
        std::optional<quic::Address> bound_on() const override { return std::nullopt; }
    };
#endif

    bool check_path(std::string path)
    {
        for (auto c : path)
        {
            if (not((c >= '0' and c <= '9') or (c >= 'A' and c <= 'Z') or (c >= 'a' and c <= 'z') or (c == '_')
                    or (c == '-')))
            {
                return false;
            }
        }

        return true;
    }

    template <typename RPC>
    void register_rpc_command(std::unordered_map<std::string, rpc_callback>& regs)
    {
        static_assert(std::is_base_of_v<RPCRequest, RPC>);
        rpc_callback cback{};

        cback.invoke = make_invoke<RPC>();

        regs.emplace(RPC::name, std::move(cback));
    }

    RPCServer::RPCServer(oxenmq::OxenMQ& omq, Router& r) : _omq{omq}, _router(r)
    {
        if (llarp::logRingBuffer)
            log_subs.emplace(_omq, llarp::logRingBuffer);

        // copied logic loop as placeholder
        for (const auto& addr : _router.config().api.rpc_bind_addrs)
        {
            _omq.listen_plain(addr.zmq_address());
            log::debug(logcat, "Bound RPC server to {}", addr.full_address());
        }

        AddCategories();
    }

    template <typename... RPC>
    std::unordered_map<std::string, rpc_callback> register_rpc_requests(tools::type_list<RPC...>)
    {
        std::unordered_map<std::string, rpc_callback> regs;

        (register_rpc_command<RPC>(regs), ...);

        return regs;
    }

    const std::unordered_map<std::string, rpc_callback> rpc_request_map =
        register_rpc_requests(rpc::rpc_request_types{});

    void RPCServer::AddCategories()
    {
        _omq.add_category("llarp", oxenmq::AuthLevel::none).add_request_command("logs", [this](oxenmq::Message& msg) {
            HandleLogsSubRequest(msg);
        });

        for (auto& req : rpc_request_map)
        {
            _omq.add_request_command(
                "llarp", req.first, [name = std::string_view{req.first}, &call = req.second, this](oxenmq::Message& m) {
                    call.invoke(m, *this);
                });
        }
    }

    void RPCServer::invoke(Halt& halt)
    {
        log_print_rpc(halt);

        if (not _router.is_running())
        {
            SetJSONError("Router is not running", halt.response);
            return;
        }

        _router.loop()->call_soon([&]() { _router.stop(); });

        SetJSONResponse("OK", halt.response);
    }

    void RPCServer::invoke(Version& version)
    {
        log_print_rpc(version);

        nlohmann::json result{{"version", llarp::LOKINET_VERSION_FULL}, {"uptime", to_json(_router.Uptime())}};

        SetJSONResponse(result, version.response);
    }

    void RPCServer::invoke(Status& status)
    {
        log_print_rpc(status);

        (_router.is_running()) ? SetJSONResponse(_router.ExtractStatus(), status.response)
                               : SetJSONError("Router is not yet ready", status.response);
    }

    void RPCServer::invoke(GetStatus& getstatus)
    {
        log_print_rpc(getstatus);

        SetJSONResponse(_router.ExtractSummaryStatus(), getstatus.response);
    }

    void RPCServer::invoke(QuicConnect& quicconnect)
    {
        log_print_rpc(quicconnect);

        auto& req = quicconnect.request;

        if (req.port == 0 and req.closeID == 0)
        {
            SetJSONError("Port not provided", quicconnect.response);
            return;
        }

        if (req.remoteHost.empty() and req.closeID == 0)
        {
            SetJSONError("Host not provided", quicconnect.response);
            return;
        }

        // auto endpoint =
        //     (req.endpoint.empty()) ? GetEndpointByName(_router, "default") : GetEndpointByName(_router,
        //     req.endpoint);

        // if (not endpoint)
        // {
        //     SetJSONError("No such local endpoint found.", quicconnect.response);
        //     return;
        // }

        // auto quic = endpoint->GetQUICTunnel();

        // if (not quic)
        // {
        //     SetJSONError("No quic interface available on endpoint " + req.endpoint, quicconnect.response);
        //     return;
        // }

        if (req.closeID)
        {
            // TODO:
            // quic->forget(req.closeID);
            SetJSONResponse("OK", quicconnect.response);
            return;
        }

        quic::Address laddr{req.bindAddr, req.port};

        try
        {
            // TODO:
            // auto [addr, id] = quic->open(
            //     req.remoteHost, req.port, [](auto&&) {}, laddr);

            nlohmann::json status;
            // status["addr"] = addr.to_string();
            // status["id"] = id;

            SetJSONResponse(status, quicconnect.response);
        }
        catch (std::exception& e)
        {
            SetJSONError(e.what(), quicconnect.response);
        }
    }

    void RPCServer::invoke(QuicListener& quiclistener)
    {
        log_print_rpc(quiclistener);

        auto req = quiclistener.request;

        if (req.port == 0 and req.closeID == 0)
        {
            SetJSONError("Invalid arguments", quiclistener.response);
            return;
        }

        // auto endpoint =
        //     (req.endpoint.empty()) ? GetEndpointByName(_router, "default") : GetEndpointByName(_router,
        //     req.endpoint);

        // if (not endpoint)
        // {
        //     SetJSONError("No such local endpoint found", quiclistener.response);
        //     return;
        // }

        // auto quic = endpoint->GetQUICTunnel();

        // if (not quic)
        // {
        //     SetJSONError("No quic interface available on endpoint " + req.endpoint, quiclistener.response);
        //     return;
        // }

        if (req.closeID)
        {
            // TODO:
            // quic->forget(req.closeID);
            SetJSONResponse("OK", quiclistener.response);
            return;
        }

        if (req.port)
        {
            auto id = 0;
            try
            {
                quic::Address addr{req.remoteHost, req.port};
                // TODO:
                // id = quic->listen(addr);
            }
            catch (std::exception& e)
            {
                SetJSONError(e.what(), quiclistener.response);
                return;
            }

            nlohmann::json result;
            result["id"] = id;
            std::string localAddress;
            // var::visit([&](auto&& addr) { localAddress = addr.to_string(); }, endpoint->local_address());
            result["addr"] = localAddress + ":" + std::to_string(req.port);

            if (not req.srvProto.empty())
            {
                dns::SRVData srvData{req.srvProto, 1, 1, req.port, ""};
                // endpoint->put_srv_record(std::move(srvData));
            }

            SetJSONResponse(result, quiclistener.response);
            return;
        }
    }

    static std::optional<NetworkAddress> try_netaddr(std::string_view x)
    {
        try
        {
            return NetworkAddress{x};
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    void RPCServer::invoke(FindCC& findcc)
    {
        log_print_rpc(findcc);

        if (_router.is_service_node())
        {
            SetJSONError("Not supported", findcc.response);
            return;
        }

        if (findcc.request.pk.empty())
        {
            SetJSONError("No pubkey provided!", findcc.response);
            return;
        }

        auto maybe_netaddr = try_netaddr(findcc.request.pk);

        if (not maybe_netaddr)
        {
            SetJSONError("Invalid pubkey provided: {}"_format(findcc.request.pk), findcc.response);
            return;
        }

        _router.loop()->call([this, netaddr = *maybe_netaddr, replier = findcc.move()]() mutable {
            _router.session_endpoint().lookup_client_intro(
                netaddr.router_id(), [&replier](std::optional<llarp::ClientContact> cc) {
                    nlohmann::json result;
                    if (cc)
                    {
                        auto cc_str = "{}"_format(*cc);
                        result.emplace("cc", cc_str);
                        log::info(logcat, "RPC call to `find_cc` returned successfully: {}", cc_str);
                    }
                    else
                    {
                        log::warning(logcat, "RPC call to `find_cc` failed!");
                        result.emplace("cc", "ERROR");
                    }
                    replier.reply(result.dump());
                });
        });
    }

    void RPCServer::invoke(SessionInit& sessioninit)
    {
        log_print_rpc(sessioninit);

        if (_router.is_service_node())
        {
            SetJSONError("Not supported", sessioninit.response);
            return;
        }

        if (sessioninit.request.pk.empty())
        {
            SetJSONError("No pubkey provided!", sessioninit.response);
            return;
        }

        auto maybe_netaddr = try_netaddr(sessioninit.request.pk);

        if (not maybe_netaddr)
        {
            SetJSONError("Invalid pubkey provided: {}"_format(sessioninit.request.pk), sessioninit.response);
            return;
        }

        _router.loop()->call([this, netaddr = *maybe_netaddr]() {
            try
            {
                log::debug(logcat, "Beginning session init to remote instance: {}", netaddr);
                _router.session_endpoint().initiate_remote_session(netaddr, nullptr);
                /*[replier = sessioninit.move()](auto success) mutable {
                    // FIXME: needs redone, initiate remote session no longer returns an ip
                    nlohmann::json result;
                    std::string a = std::holds_alternative<ipv4>(ip) ? std::get<ipv4>(ip).to_string()
                                                                     : std::get<ipv6>(ip).to_string();
                    result.emplace("ip", a);
                    log::info(logcat, "RPC call to `session_init` succeeded: {}", a);
                    replier.reply(result.dump());
                });
                */
                log::info(logcat, "RPC Server dispatched `session_init` to remote:{}", netaddr);
            }
            catch (const std::exception& e)
            {
                log::critical(logcat, "Failed to parse remote instance netaddr: {}", e.what());
            }
        });
    }

    void RPCServer::invoke(SessionClose& sessionclose)
    {
        log_print_rpc(sessionclose);

        if (sessionclose.request.pk.empty())
        {
            SetJSONError("No pubkey provided!", sessionclose.response);
            return;
        }

        auto maybe_netaddr = try_netaddr(sessionclose.request.pk);

        if (not maybe_netaddr)
        {
            SetJSONError("Invalid pubkey provided: {}"_format(sessionclose.request.pk), sessionclose.response);
            return;
        }

        auto& netaddr = *maybe_netaddr;

        _router.loop()->call([&]() {
            try
            {
                if (auto session = _router.session_endpoint().get_session(netaddr))
                {
                    auto hook = [replier = sessionclose.move()](quic::message m) mutable {
                        nlohmann::json result;

                        if (m)
                        {
                            result.emplace("result", "OK");
                            log::info(logcat, "RPC call to `session_close` succeeded!");
                            return replier.reply(result.dump());
                        }

                        std::string status{"<none given>"};

                        try
                        {
                            oxenc::bt_dict_consumer btdc{m.body()};

                            if (auto s = btdc.maybe<std::string>(messages::STATUS_KEY))
                                status = std::move(*s);
                        }
                        catch (const std::exception& e)
                        {
                            status = "Exception: {}"_format(e.what());
                        }

                        log::critical(logcat, "Call to `session_close` FAILED; reason: {}", status);
                        result.emplace("result", std::move(status));
                        replier.reply(result.dump());
                    };

                    session->stop_session(true, std::move(hook));

                    log::info(logcat, "RPC Server dispatched `session_close` to remote:{}", netaddr);
                }
            }
            catch (const std::exception& e)
            {
                log::critical(logcat, "Failed to parse remote instance netaddr: {}", e.what());
            }
        });
    }

    // TODO: fix this because it's bad
    void RPCServer::invoke(LookupSnode& lookupsnode)
    {
        log_print_rpc(lookupsnode);

        if (not _router.is_service_node())
        {
            SetJSONError("Not supported", lookupsnode.response);
            return;
        }

        RouterID routerID;

        if (lookupsnode.request.routerID.empty())
        {
            SetJSONError("No remote ID provided", lookupsnode.response);
            return;
        }

        if (not routerID.from_relay_address(lookupsnode.request.routerID))
        {
            SetJSONError("Invalid remote: " + lookupsnode.request.routerID, lookupsnode.response);
            return;
        }

        // _router.loop()->call([&]() {
        //   auto endpoint = _router.exit_context().get_exit_endpoint("default");

        //   if (endpoint == nullptr)
        //   {
        //     SetJSONError("Cannot find local endpoint: default", lookupsnode.response);
        //     return;
        //   }

        //   endpoint->ObtainSNodeSession(routerID, [&](auto session) {
        //     if (session and session->IsReady())
        //     {
        //       const auto ip = net::TruncateV6(endpoint->GetIPForIdent(PubKey{routerID}));
        //       nlohmann::json status{{"ip", ip.to_string()}};
        //       SetJSONResponse(status, lookupsnode.response);
        //       return;
        //     }

        //     SetJSONError("Failed to obtain snode session", lookupsnode.response);
        //     return;
        //   });
        // });
    }

    void RPCServer::invoke(MapExit& mapexit)
    {
        log_print_rpc(mapexit);

        MapExit exit_request;
        // steal replier from exit RPC endpoint
        exit_request.replier.emplace(mapexit.move());

        // TODO: connect this to remote service session management (service::Handler)
        // _router.hidden_service_context().GetDefault()->map_exit(
        //     mapexit.request.address,
        //     mapexit.request.token,
        //     mapexit.request.ip_range,
        //     [exit = std::move(exit_request)](bool success, std::string result) mutable {
        //       if (success)
        //         exit.send_response({{"result"}, std::move(result)});
        //       else
        //         exit.send_response({{"error"}, std::move(result)});
        //     });
    }

    void RPCServer::invoke(ListExits& listexits)
    {
        log_print_rpc(listexits);

        (void)listexits;
        // if (not _router.hidden_service_context().hasEndpoints())
        // {
        //   SetJSONError("No mapped endpoints found", listexits.response);
        //   return;
        // }

        // auto status = _router.hidden_service_context().GetDefault()->ExtractStatus()["exitMap"];

        // SetJSONResponse((status.empty()) ? "No exits" : status, listexits.response);
    }

    void RPCServer::invoke(UnmapExit& unmapexit)
    {
        log_print_rpc(unmapexit);

        try
        {
            // for (auto& ip : unmapexit.request.ip_range)
            //   _router.hidden_service_context().GetDefault()->UnmapExitRange(ip);
        }
        catch (std::exception& e)
        {
            SetJSONError("Unable to unmap to given range", unmapexit.response);
            return;
        }

        SetJSONResponse("OK", unmapexit.response);
    }

    //  Sequentially calls map_exit and unmap_exit to hotswap mapped connection from old exit
    //  to new exit. Similar to how map_exit steals the oxenmq deferredsend object, swapexit
    //  moves the replier object to the unmap_exit struct, as that is called second. Rather than
    //  the nested lambda within map_exit making the reply call, it instead calls the unmap_exit
    //  logic and leaves the message handling to the unmap_exit struct
    void RPCServer::invoke(SwapExits& swapexits)
    {
        log_print_rpc(swapexits);

        (void)swapexits;
        // MapExit map_request;
        // UnmapExit unmap_request;
        // auto endpoint = _router.hidden_service_context().GetDefault();
        // auto current_exits = endpoint->ExtractStatus()["exitMap"];

        // if (current_exits.empty())
        // {
        //   SetJSONError("Cannot swap to new exit: no exits currently mapped", swapexits.response);
        //   return;
        // }

        // if (swapexits.request.exit_addresses.size() < 2)
        // {
        //   SetJSONError("Exit addresses not passed", swapexits.response);
        //   return;
        // }

        // // steal replier from swapexit RPC endpoint
        // unmap_request.replier.emplace(swapexits.move());

        // // set map_exit request to new address
        // map_request.request.address = swapexits.request.exit_addresses[1];

        // // set token for new exit node mapping
        // if (not swapexits.request.token.empty())
        //   map_request.request.token = swapexits.request.token;

        // // populate map_exit request with old IP ranges
        // for (auto& [range, exit] : current_exits.items())
        // {
        //   if (exit.get<std::string>() == swapexits.request.exit_addresses[0])
        //   {
        //     map_request.request.ip_range.emplace_back(range);
        //     unmap_request.request.ip_range.emplace_back(range);
        //   }
        // }

        // if (map_request.request.ip_range.empty() or unmap_request.request.ip_range.empty())
        // {
        //   SetJSONError("No mapped ranges found matching requested swap", swapexits.response);
        //   return;
        // }

        // endpoint->map_exit(
        //     map_request.request.address,
        //     map_request.request.token,
        //     map_request.request.ip_range,
        //     [unmap = std::move(unmap_request),
        //      ep = endpoint,
        //      old_exit = swapexits.request.exit_addresses[0]](bool success, std::string result)
        //      mutable {
        //       if (not success)
        //         unmap.send_response({{"error"}, std::move(result)});
        //       else
        //       {
        //         try
        //         {
        //           for (auto& ip : unmap.request.ip_range)
        //             ep->UnmapRangeByExit(ip, old_exit);
        //         }
        //         catch (std::exception& e)
        //         {
        //           SetJSONError("Unable to unmap to given range", unmap.response);
        //           return;
        //         }

        //         SetJSONResponse("OK", unmap.response);
        //         unmap.send_response();
        //       }
        //     });
    }

#ifndef LOKINET_LIBRARY_ONLY
    void RPCServer::invoke(DNSQuery& dnsquery)
    {
        log_print_rpc(dnsquery);

        std::string qname = (dnsquery.request.qname.empty()) ? "" : dnsquery.request.qname;
        dns::QType_t qtype = (dnsquery.request.qtype) ? dnsquery.request.qtype : dns::qTypeA;

        dns::Message msg{dns::Question{qname, qtype}};

        // auto endpoint = (dnsquery.request.endpoint.empty()) ? GetEndpointByName(_router, "default")
        //                                                     : GetEndpointByName(_router, dnsquery.request.endpoint);

        // if (endpoint == nullptr)
        // {
        //     SetJSONError("No such endpoint found for dns query", dnsquery.response);
        //     return;
        // }

        // if (auto dns = endpoint->DNS())
        // {
        //     auto packet_src = std::make_shared<DummyPacketSource>([&](auto result) {
        //         if (result)
        //             SetJSONResponse(result->ToJSON(), dnsquery.response);
        //         else
        //             SetJSONError("No response from DNS", dnsquery.response);
        //     });
        //     if (not dns->maybe_handle_packet(packet_src, packet_src->dumb, packet_src->dumb,
        //     IPPacket{msg.to_buffer()}))
        //         SetJSONError("DNS query not accepted by endpoint", dnsquery.response);
        // }
        // else
        //     SetJSONError("Endpoint does not have dns", dnsquery.response);
        return;
    }
#endif

    void RPCServer::invoke(Config& config)
    {
        log_print_rpc(config);

        if (config.request.filename.empty() and not config.request.ini.empty())
        {
            SetJSONError("No filename specified for .ini file", config.response);
            return;
        }
        if (config.request.ini.empty() and not config.request.filename.empty())
        {
            SetJSONError("No .ini chunk provided", config.response);
            return;
        }

        if (config.request.filename.ends_with(".ini"))
        {
            SetJSONError("Must append '.ini' to filename", config.response);
            return;
        }

        if (not check_path(config.request.filename))
        {
            SetJSONError("Bad filename passed", config.response);
            return;
        }

        fs::path conf_d{"conf.d"};

        if (config.request.del and not config.request.filename.empty())
        {
            try
            {
                if (fs::exists(conf_d / (config.request.filename)))
                    fs::remove(conf_d / (config.request.filename));
            }
            catch (std::exception& e)
            {
                SetJSONError(e.what(), config.response);
                return;
            }
        }
        else
        {
            try
            {
                if (not fs::exists(conf_d))
                    fs::create_directory(conf_d);

                auto parser = ConfigParser();

                if (parser.load_new_from_str(config.request.ini))
                {
                    parser.set_filename(conf_d / (config.request.filename));
                    parser.save_new();
                }
            }
            catch (std::exception& e)
            {
                SetJSONError(e.what(), config.response);
                return;
            }
        }

        SetJSONResponse("OK", config.response);
    }

    void RPCServer::HandleLogsSubRequest(oxenmq::Message& m)
    {
        if (m.data.size() != 1)
        {
            m.send_reply("Invalid subscription request: no log receipt endpoint given");
            return;
        }

        if (!log_subs)
        {
            m.send_reply("This Lokinet instance is not capturing logs");
            return;
        }

        auto endpoint = std::string{m.data[0]};

        if (endpoint == "unsubscribe")
        {
            log::debug(logcat, "New logs unsubscribe request from conn {}@{}", m.conn.to_string(), m.remote);
            log_subs->unsubscribe(m.conn);
            m.send_reply("OK");
            return;
        }

        auto is_new = log_subs->subscribe(m.conn, endpoint);

        if (is_new)
        {
            log::debug(logcat, "New logs subscription request from conn {}@{}", m.conn.to_string(), m.remote);
            m.send_reply("OK");
            log_subs->send_all(m.conn, endpoint);
        }
        else
        {
            log::debug(logcat, "Renewed logs subscription request from conn id {}@{}", m.conn.to_string(), m.remote);
            m.send_reply("ALREADY");
        }
    }

}  // namespace llarp::rpc
