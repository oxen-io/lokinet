#include "rpc_server.hpp"

#include "rpc_request.hpp"

#include <llarp/config/config.hpp>
#include <llarp/config/ini.hpp>
#include <llarp/constants/version.hpp>
#include <llarp/dns/dns.hpp>
#include <llarp/exit/context.hpp>
#include <llarp/net/ip_range.hpp>
#include <llarp/router/router.hpp>
#include <llarp/rpc/rpc_request_definitions.hpp>
#include <llarp/service/context.hpp>

#include <nlohmann/json.hpp>

#include <exception>
#include <vector>

namespace llarp::rpc
{
  // Fake packet source that serializes repsonses back into dns
  class DummyPacketSource : public dns::PacketSource_Base
  {
    std::function<void(std::optional<dns::Message>)> func;

   public:
    SockAddr dumb;

    template <typename Callable>
    DummyPacketSource(Callable&& f) : func{std::forward<Callable>(f)}
    {}

    bool
    WouldLoop(const SockAddr&, const SockAddr&) const override
    {
      return false;
    };

    /// send packet with src and dst address containing buf on this packet source
    void
    SendTo(const SockAddr&, const SockAddr&, OwnedBuffer buf) const override
    {
      func(dns::MaybeParseDNSMessage(buf));
    }

    /// stop reading packets and end operation
    void
    Stop() override{};

    /// returns the sockaddr we are bound on if applicable
    std::optional<SockAddr>
    BoundOn() const override
    {
      return std::nullopt;
    }
  };

  bool
  check_path(std::string path)
  {
    for (auto c : path)
    {
      if (not((c >= '0' and c <= '9') or (c >= 'A' and c <= 'Z') or (c >= 'a' and c <= 'z')
              or (c == '_') or (c == '-')))
      {
        return false;
      }
    }

    return true;
  }

  std::shared_ptr<EndpointBase>
  GetEndpointByName(Router& r, std::string name)
  {
    if (r.is_service_node())
    {
      return r.exitContext().get_exit_endpoint(name);
    }

    return r.hidden_service_context().GetEndpointByName(name);
  }

  template <typename RPC>
  void
  register_rpc_command(std::unordered_map<std::string, rpc_callback>& regs)
  {
    static_assert(std::is_base_of_v<RPCRequest, RPC>);
    rpc_callback cback{};

    cback.invoke = make_invoke<RPC>();

    regs.emplace(RPC::name, std::move(cback));
  }

  RPCServer::RPCServer(LMQ_ptr lmq, Router& r)
      : m_LMQ{std::move(lmq)}, m_Router(r), log_subs{*m_LMQ, llarp::logRingBuffer}
  {
    // copied logic loop as placeholder
    for (const auto& addr : r.config()->api.rpc_bind_addrs)
    {
      m_LMQ->listen_plain(addr.zmq_address());
      LogInfo("Bound RPC server to ", addr.full_address());
    }

    AddCategories();
  }

  template <typename... RPC>
  std::unordered_map<std::string, rpc_callback>
  register_rpc_requests(tools::type_list<RPC...>)
  {
    std::unordered_map<std::string, rpc_callback> regs;

    (register_rpc_command<RPC>(regs), ...);

    return regs;
  }

  const std::unordered_map<std::string, rpc_callback> rpc_request_map =
      register_rpc_requests(rpc::rpc_request_types{});

  void
  RPCServer::AddCategories()
  {
    m_LMQ->add_category("llarp", oxenmq::AuthLevel::none)
        .add_request_command("logs", [this](oxenmq::Message& msg) { HandleLogsSubRequest(msg); });

    for (auto& req : rpc_request_map)
    {
      m_LMQ->add_request_command(
          "llarp",
          req.first,
          [name = std::string_view{req.first}, &call = req.second, this](oxenmq::Message& m) {
            call.invoke(m, *this);
          });
    }
  }

  void
  RPCServer::invoke(Halt& halt)
  {
    if (not m_Router.IsRunning())
    {
      SetJSONError("Router is not running", halt.response);
      return;
    }
    SetJSONResponse("OK", halt.response);
    m_Router.Stop();
  }

  void
  RPCServer::invoke(Version& version)
  {
    util::StatusObject result{
        {"version", llarp::LOKINET_VERSION_FULL}, {"uptime", to_json(m_Router.Uptime())}};

    SetJSONResponse(result, version.response);
  }

  void
  RPCServer::invoke(Status& status)
  {
    (m_Router.IsRunning()) ? SetJSONResponse(m_Router.ExtractStatus(), status.response)
                           : SetJSONError("Router is not yet ready", status.response);
  }

  void
  RPCServer::invoke(GetStatus& getstatus)
  {
    SetJSONResponse(m_Router.ExtractSummaryStatus(), getstatus.response);
  }

  void
  RPCServer::invoke(QuicConnect& quicconnect)
  {
    if (quicconnect.request.port == 0 and quicconnect.request.closeID == 0)
    {
      SetJSONError("Port not provided", quicconnect.response);
      return;
    }

    if (quicconnect.request.remoteHost.empty() and quicconnect.request.closeID == 0)
    {
      SetJSONError("Host not provided", quicconnect.response);
      return;
    }

    auto endpoint = (quicconnect.request.endpoint.empty())
        ? GetEndpointByName(m_Router, "default")
        : GetEndpointByName(m_Router, quicconnect.request.endpoint);

    if (not endpoint)
    {
      SetJSONError("No such local endpoint found.", quicconnect.response);
      return;
    }

    auto quic = endpoint->GetQUICTunnel();

    if (not quic)
    {
      SetJSONError(
          "No quic interface available on endpoint " + quicconnect.request.endpoint,
          quicconnect.response);
      return;
    }

    if (quicconnect.request.closeID)
    {
      // TODO:
      // quic->forget(quicconnect.request.closeID);
      SetJSONResponse("OK", quicconnect.response);
      return;
    }

    SockAddr laddr{quicconnect.request.bindAddr};

    try
    {
      // TODO:
      // auto [addr, id] = quic->open(
      //     quicconnect.request.remoteHost, quicconnect.request.port, [](auto&&) {}, laddr);

      util::StatusObject status;
      // status["addr"] = addr.ToString();
      // status["id"] = id;

      SetJSONResponse(status, quicconnect.response);
    }
    catch (std::exception& e)
    {
      SetJSONError(e.what(), quicconnect.response);
    }
  }

  void
  RPCServer::invoke(QuicListener& quiclistener)
  {
    if (quiclistener.request.port == 0 and quiclistener.request.closeID == 0)
    {
      SetJSONError("Invalid arguments", quiclistener.response);
      return;
    }

    auto endpoint = (quiclistener.request.endpoint.empty())
        ? GetEndpointByName(m_Router, "default")
        : GetEndpointByName(m_Router, quiclistener.request.endpoint);

    if (not endpoint)
    {
      SetJSONError("No such local endpoint found", quiclistener.response);
      return;
    }

    auto quic = endpoint->GetQUICTunnel();

    if (not quic)
    {
      SetJSONError(
          "No quic interface available on endpoint " + quiclistener.request.endpoint,
          quiclistener.response);
      return;
    }

    if (quiclistener.request.closeID)
    {
      // TODO:
      // quic->forget(quiclistener.request.closeID);
      SetJSONResponse("OK", quiclistener.response);
      return;
    }

    if (quiclistener.request.port)
    {
      auto id = 0;
      try
      {
        SockAddr addr{quiclistener.request.remoteHost, huint16_t{quiclistener.request.port}};
        // TODO:
        // id = quic->listen(addr);
      }
      catch (std::exception& e)
      {
        SetJSONError(e.what(), quiclistener.response);
        return;
      }

      util::StatusObject result;
      result["id"] = id;
      std::string localAddress;
      var::visit([&](auto&& addr) { localAddress = addr.ToString(); }, endpoint->LocalAddress());
      result["addr"] = localAddress + ":" + std::to_string(quiclistener.request.port);

      if (not quiclistener.request.srvProto.empty())
      {
        auto srvData = dns::SRVData::fromTuple(
            std::make_tuple(quiclistener.request.srvProto, 1, 1, quiclistener.request.port, ""));
        endpoint->PutSRVRecord(std::move(srvData));
      }

      SetJSONResponse(result, quiclistener.response);
      return;
    }
  }

  // TODO: fix this because it's bad
  void
  RPCServer::invoke(LookupSnode& lookupsnode)
  {
    if (not m_Router.is_service_node())
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

    if (not routerID.from_string(lookupsnode.request.routerID))
    {
      SetJSONError("Invalid remote: " + lookupsnode.request.routerID, lookupsnode.response);
      return;
    }

    m_Router.loop()->call([&]() {
      auto endpoint = m_Router.exitContext().get_exit_endpoint("default");

      if (endpoint == nullptr)
      {
        SetJSONError("Cannot find local endpoint: default", lookupsnode.response);
        return;
      }

      endpoint->ObtainSNodeSession(routerID, [&](auto session) {
        if (session and session->IsReady())
        {
          const auto ip = net::TruncateV6(endpoint->GetIPForIdent(PubKey{routerID}));
          util::StatusObject status{{"ip", ip.ToString()}};
          SetJSONResponse(status, lookupsnode.response);
          return;
        }

        SetJSONError("Failed to obtain snode session", lookupsnode.response);
        return;
      });
    });
  }

  void
  RPCServer::invoke(MapExit& mapexit)
  {
    MapExit exit_request;
    // steal replier from exit RPC endpoint
    exit_request.replier.emplace(mapexit.move());

    m_Router.hidden_service_context().GetDefault()->map_exit(
        mapexit.request.address,
        mapexit.request.token,
        mapexit.request.ip_range,
        [exit = std::move(exit_request)](bool success, std::string result) mutable {
          if (success)
            exit.send_response({{"result"}, std::move(result)});
          else
            exit.send_response({{"error"}, std::move(result)});
        });
  }

  void
  RPCServer::invoke(ListExits& listexits)
  {
    if (not m_Router.hidden_service_context().hasEndpoints())
    {
      SetJSONError("No mapped endpoints found", listexits.response);
      return;
    }

    auto status = m_Router.hidden_service_context().GetDefault()->ExtractStatus()["exitMap"];

    SetJSONResponse((status.empty()) ? "No exits" : status, listexits.response);
  }

  void
  RPCServer::invoke(UnmapExit& unmapexit)
  {
    try
    {
      for (auto& ip : unmapexit.request.ip_range)
        m_Router.hidden_service_context().GetDefault()->UnmapExitRange(ip);
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
  //  the nested lambda within map_exit making the reply call, it instead calls the unmap_exit logic
  //  and leaves the message handling to the unmap_exit struct
  void
  RPCServer::invoke(SwapExits& swapexits)
  {
    MapExit map_request;
    UnmapExit unmap_request;
    auto endpoint = m_Router.hidden_service_context().GetDefault();
    auto current_exits = endpoint->ExtractStatus()["exitMap"];

    if (current_exits.empty())
    {
      SetJSONError("Cannot swap to new exit: no exits currently mapped", swapexits.response);
      return;
    }

    if (swapexits.request.exit_addresses.size() < 2)
    {
      SetJSONError("Exit addresses not passed", swapexits.response);
      return;
    }

    // steal replier from swapexit RPC endpoint
    unmap_request.replier.emplace(swapexits.move());

    // set map_exit request to new address
    map_request.request.address = swapexits.request.exit_addresses[1];

    // set token for new exit node mapping
    if (not swapexits.request.token.empty())
      map_request.request.token = swapexits.request.token;

    // populate map_exit request with old IP ranges
    for (auto& [range, exit] : current_exits.items())
    {
      if (exit.get<std::string>() == swapexits.request.exit_addresses[0])
      {
        map_request.request.ip_range.emplace_back(range);
        unmap_request.request.ip_range.emplace_back(range);
      }
    }

    if (map_request.request.ip_range.empty() or unmap_request.request.ip_range.empty())
    {
      SetJSONError("No mapped ranges found matching requested swap", swapexits.response);
      return;
    }

    endpoint->map_exit(
        map_request.request.address,
        map_request.request.token,
        map_request.request.ip_range,
        [unmap = std::move(unmap_request),
         ep = endpoint,
         old_exit = swapexits.request.exit_addresses[0]](bool success, std::string result) mutable {
          if (not success)
            unmap.send_response({{"error"}, std::move(result)});
          else
          {
            try
            {
              for (auto& ip : unmap.request.ip_range)
                ep->UnmapRangeByExit(ip, old_exit);
            }
            catch (std::exception& e)
            {
              SetJSONError("Unable to unmap to given range", unmap.response);
              return;
            }

            SetJSONResponse("OK", unmap.response);
            unmap.send_response();
          }
        });
  }

  void
  RPCServer::invoke(DNSQuery& dnsquery)
  {
    std::string qname = (dnsquery.request.qname.empty()) ? "" : dnsquery.request.qname;
    dns::QType_t qtype = (dnsquery.request.qtype) ? dnsquery.request.qtype : dns::qTypeA;

    dns::Message msg{dns::Question{qname, qtype}};

    auto endpoint = (dnsquery.request.endpoint.empty())
        ? GetEndpointByName(m_Router, "default")
        : GetEndpointByName(m_Router, dnsquery.request.endpoint);

    if (endpoint == nullptr)
    {
      SetJSONError("No such endpoint found for dns query", dnsquery.response);
      return;
    }

    if (auto dns = endpoint->DNS())
    {
      auto packet_src = std::make_shared<DummyPacketSource>([&](auto result) {
        if (result)
          SetJSONResponse(result->ToJSON(), dnsquery.response);
        else
          SetJSONError("No response from DNS", dnsquery.response);
      });
      if (not dns->MaybeHandlePacket(
              packet_src, packet_src->dumb, packet_src->dumb, msg.ToBuffer()))
        SetJSONError("DNS query not accepted by endpoint", dnsquery.response);
    }
    else
      SetJSONError("Endpoint does not have dns", dnsquery.response);
    return;
  }

  void
  RPCServer::invoke(Config& config)
  {
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

    if (not ends_with(config.request.filename, ".ini"))
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

  void
  RPCServer::HandleLogsSubRequest(oxenmq::Message& m)
  {
    if (m.data.size() != 1)
    {
      m.send_reply("Invalid subscription request: no log receipt endpoint given");
      return;
    }

    auto endpoint = std::string{m.data[0]};

    if (endpoint == "unsubscribe")
    {
      log::info(
          logcat, "New logs unsubscribe request from conn {}@{}", m.conn.to_string(), m.remote);
      log_subs.unsubscribe(m.conn);
      m.send_reply("OK");
      return;
    }

    auto is_new = log_subs.subscribe(m.conn, endpoint);

    if (is_new)
    {
      log::info(
          logcat, "New logs subscription request from conn {}@{}", m.conn.to_string(), m.remote);
      m.send_reply("OK");
      log_subs.send_all(m.conn, endpoint);
    }
    else
    {
      log::debug(
          logcat,
          "Renewed logs subscription request from conn id {}@{}",
          m.conn.to_string(),
          m.remote);
      m.send_reply("ALREADY");
    }
  }

}  // namespace llarp::rpc
