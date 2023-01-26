#include "rpc_server.hpp"
#include "llarp/rpc/rpc_request_definitions.hpp"
#include "rpc_request.hpp"
#include "llarp/service/address.hpp"
#include <exception>
#include <llarp/router/route_poker.hpp>
#include <llarp/config/config.hpp>
#include <llarp/config/ini.hpp>
#include <llarp/constants/platform.hpp>
#include <llarp/constants/version.hpp>
#include <nlohmann/json.hpp>
#include <llarp/exit/context.hpp>
#include <llarp/net/ip_range.hpp>
#include <llarp/quic/tunnel.hpp>
#include <llarp/service/context.hpp>
#include <llarp/service/outbound_context.hpp>
#include <llarp/service/auth.hpp>
#include <llarp/service/name.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/dns/dns.hpp>
#include <vector>
#include <oxenmq/fmt.h>

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
  GetEndpointByName(AbstractRouter& r, std::string name)
  {
    if (r.IsServiceNode())
    {
      return r.exitContext().GetExitEndpoint(name);
    }

    return r.hiddenServiceContext().GetEndpointByName(name);
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

  RPCServer::RPCServer(LMQ_ptr lmq, AbstractRouter& r)
      : m_LMQ{std::move(lmq)}, m_Router(r), log_subs{*m_LMQ, llarp::logRingBuffer}
  {
    // copied logic loop as placeholder
    for (const auto& addr : r.GetConfig()->api.m_rpcBindAddresses)
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
      halt.response = CreateJSONError("Router is not running");
      return;
    }
    halt.response = CreateJSONResponse("OK");
    m_Router.Stop();
  }

  void
  RPCServer::invoke(Version& version)
  {
    util::StatusObject result{
        {"version", llarp::VERSION_FULL}, {"uptime", to_json(m_Router.Uptime())}};

    version.response = CreateJSONResponse(result);
  }

  void
  RPCServer::invoke(Status& status)
  {
    status.response = (m_Router.IsRunning()) ? CreateJSONResponse(m_Router.ExtractStatus())
                                             : CreateJSONError("Router is not yet ready");
  }

  void
  RPCServer::invoke(GetStatus& getstatus)
  {
    getstatus.response = CreateJSONResponse(m_Router.ExtractSummaryStatus());
  }

  void
  RPCServer::invoke(QuicConnect& quicconnect)
  {
    if (quicconnect.request.port == 0 and quicconnect.request.closeID == 0)
    {
      quicconnect.response = CreateJSONError("Port not provided");
      return;
    }

    if (quicconnect.request.remoteHost.empty() and quicconnect.request.closeID == 0)
    {
      quicconnect.response = CreateJSONError("Host not provided");
      return;
    }

    auto endpoint = (quicconnect.request.endpoint.empty())
        ? GetEndpointByName(m_Router, "default")
        : GetEndpointByName(m_Router, quicconnect.request.endpoint);

    if (not endpoint)
    {
      quicconnect.response = CreateJSONError("No such local endpoint found.");
      return;
    }

    auto quic = endpoint->GetQUICTunnel();

    if (not quic)
    {
      quicconnect.response = CreateJSONError(
          "No quic interface available on endpoint " + quicconnect.request.endpoint);
      return;
    }

    if (quicconnect.request.closeID)
    {
      quic->forget(quicconnect.request.closeID);
      quicconnect.response = CreateJSONResponse("OK");
      return;
    }

    SockAddr laddr{quicconnect.request.bindAddr};

    try
    {
      auto [addr, id] = quic->open(
          quicconnect.request.remoteHost, quicconnect.request.port, [](auto&&) {}, laddr);

      util::StatusObject status;
      status["addr"] = addr.ToString();
      status["id"] = id;

      quicconnect.response = CreateJSONResponse(status);
    }
    catch (std::exception& e)
    {
      quicconnect.response = CreateJSONError(e.what());
    }
  }

  void
  RPCServer::invoke(QuicListener& quiclistener)
  {
    if (quiclistener.request.port == 0 and quiclistener.request.closeID == 0)
    {
      quiclistener.response = CreateJSONError("Invalid arguments");
      return;
    }

    auto endpoint = (quiclistener.request.endpoint.empty())
        ? GetEndpointByName(m_Router, "default")
        : GetEndpointByName(m_Router, quiclistener.request.endpoint);

    if (not endpoint)
    {
      quiclistener.response = CreateJSONError("No such local endpoint found");
      return;
    }

    auto quic = endpoint->GetQUICTunnel();

    if (not quic)
    {
      quiclistener.response = CreateJSONError(
          "No quic interface available on endpoint " + quiclistener.request.endpoint);
      return;
    }

    if (quiclistener.request.closeID)
    {
      quic->forget(quiclistener.request.closeID);
      quiclistener.response = CreateJSONResponse("OK");
      return;
    }

    if (quiclistener.request.port)
    {
      auto id = 0;
      try
      {
        SockAddr addr{quiclistener.request.remoteHost, huint16_t{quiclistener.request.port}};
        id = quic->listen(addr);
      }
      catch (std::exception& e)
      {
        quiclistener.response = CreateJSONError(e.what());
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

      quiclistener.response = CreateJSONResponse(result);
      return;
    }
  }

  void
  RPCServer::invoke(LookupSnode& lookupsnode)
  {
    if (not m_Router.IsServiceNode())
    {
      lookupsnode.response = CreateJSONError("Not supported");
      return;
    }

    RouterID routerID;
    if (lookupsnode.request.routerID.empty())
    {
      lookupsnode.response = CreateJSONError("No remote ID provided");
      return;
    }

    if (not routerID.FromString(lookupsnode.request.routerID))
    {
      lookupsnode.response = CreateJSONError("Invalid remote: " + lookupsnode.request.routerID);
      return;
    }

    m_Router.loop()->call([&]() {
      auto endpoint = m_Router.exitContext().GetExitEndpoint("default");

      if (endpoint == nullptr)
      {
        lookupsnode.response = CreateJSONError("Cannot find local endpoint: default");
        return;
      }

      endpoint->ObtainSNodeSession(routerID, [&](auto session) {
        if (session and session->IsReady())
        {
          const auto ip = net::TruncateV6(endpoint->GetIPForIdent(PubKey{routerID}));
          util::StatusObject status{{"ip", ip.ToString()}};
          lookupsnode.response = CreateJSONResponse(status);
          return;
        }

        lookupsnode.response = CreateJSONError("Failed to obtain snode session");
        return;
      });
    });
  }

  void
  RPCServer::invoke(MapExit& mapexit)
  {
    MapExit exit_request;
    //  steal replier from exit RPC endpoint
    exit_request.replier.emplace(std::move(*mapexit.replier));

    m_Router.hiddenServiceContext().GetDefault()->map_exit(
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
    if (not m_Router.hiddenServiceContext().hasEndpoints())
      listexits.response = CreateJSONError("No mapped endpoints found");
    else
      listexits.response =
          CreateJSONResponse(m_Router.hiddenServiceContext().GetDefault()->ExtractStatus()["m_"
                                                                                           "ExitMa"
                                                                                           "p"]);
  }

  void
  RPCServer::invoke(UnmapExit& unmapexit)
  {
    if (unmapexit.request.ip_range.empty())
    {
      unmapexit.response = CreateJSONError("No IP range provided");
      return;
    }

    try
    {
      m_Router.routePoker()->Down();
      for (auto& ip : unmapexit.request.ip_range)
        m_Router.hiddenServiceContext().GetDefault()->UnmapExitRange(ip);
    }
    catch (std::exception& e)
    {
      unmapexit.response = CreateJSONError("Unable to unmap to given range");
    }

    unmapexit.response = CreateJSONResponse("OK");
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
      dnsquery.response = CreateJSONError("No such endpoint found for dns query");
      return;
    }

    if (auto dns = endpoint->DNS())
    {
      auto packet_src = std::make_shared<DummyPacketSource>([&](auto result) {
        if (result)
          dnsquery.response = CreateJSONResponse(result->ToJSON());
        else
          dnsquery.response = CreateJSONError("No response from DNS");
      });
      if (not dns->MaybeHandlePacket(
              packet_src, packet_src->dumb, packet_src->dumb, msg.ToBuffer()))
        dnsquery.response = CreateJSONError("DNS query not accepted by endpoint");
    }
    else
      dnsquery.response = CreateJSONError("Endpoint does not have dns");
    return;
  }

  void
  RPCServer::invoke(Config& config)
  {
    if (config.request.filename.empty() and not config.request.ini.empty())
    {
      config.response = CreateJSONError("No filename specified for .ini file");
      return;
    }
    if (config.request.ini.empty() and not config.request.filename.empty())
    {
      config.response = CreateJSONError("No .ini chunk provided");
      return;
    }

    if (not ends_with(config.request.filename, ".ini"))
    {
      config.response = CreateJSONError("Must append '.ini' to filename");
      return;
    }

    if (not check_path(config.request.filename))
    {
      config.response = CreateJSONError("Bad filename passed");
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
        config.response = CreateJSONError(e.what());
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

        if (parser.LoadNewFromStr(config.request.ini))
        {
          parser.Filename(conf_d / (config.request.filename));
          parser.SaveNew();
        }
      }
      catch (std::exception& e)
      {
        config.response = CreateJSONError(e.what());
        return;
      }
    }

    config.response = CreateJSONResponse("OK");
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
      log::info(logcat, "New logs unsubscribe request from conn {}@{}", m.conn, m.remote);
      log_subs.unsubscribe(m.conn);
      m.send_reply("OK");
      return;
    }

    auto is_new = log_subs.subscribe(m.conn, endpoint);

    if (is_new)
    {
      log::info(logcat, "New logs subscription request from conn {}@{}", m.conn, m.remote);
      m.send_reply("OK");
      log_subs.send_all(m.conn, endpoint);
    }
    else
    {
      log::debug(logcat, "Renewed logs subscription request from conn id {}@{}", m.conn, m.remote);
      m.send_reply("ALREADY");
    }
  }

}  // namespace llarp::rpc