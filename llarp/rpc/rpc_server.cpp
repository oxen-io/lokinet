#include "rpc_server.hpp"
#include "rpc_request.hpp"
#include <cmath>
#include <exception>
#include <llarp/router/route_poker.hpp>
#include <llarp/config/config.hpp>
#include <llarp/config/ini.hpp>
#include <llarp/constants/platform.hpp>
#include <llarp/constants/version.hpp>
#include <nlohmann/json.hpp>
#include <llarp/net/ip_range.hpp>
#include <llarp/quic/tunnel.hpp>
#include <llarp/service/outbound_context.hpp>
#include <llarp/service/auth.hpp>
#include <llarp/service/name.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/layers/layers.hpp>
#include <llarp/layers/platform/dns_bridge.hpp>
#include <llarp/dns/server.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <vector>
#include <oxenmq/fmt.h>

namespace llarp::rpc
{
  // Fake packet source that serializes repsonses back into dns
  class DummyPacketSource : public dns::PacketSource_Base
  {
    std::function<void(dns::Message)> func;

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
      auto view = buf.view();
      func(dns::Message{view});
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

  namespace
  {
    /// make sure we have 'endpoint' set right
    template <typename RPC_t>
    [[nodiscard]] bool
    check_endpoint_param(RPC_t& rpc)
    {
      const auto& name = rpc.request.endpoint;
      // only allow endpoint named 'default' or empty string.
      bool ok = name.empty() or name == "default";
      if (not ok)
        SetJSONError("No such local endpoint found.", rpc.response);
      return ok;
    }
  }  // namespace

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
        {"version", llarp::VERSION_FULL}, {"uptime", to_json(m_Router.Uptime())}};

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

    if (not check_endpoint_param(quicconnect))
      return;

    auto quic = m_Router.quic_tunnel();

    if (not quic)
    {
      SetJSONError(
          "No quic interface available on endpoint " + quicconnect.request.endpoint,
          quicconnect.response);
      return;
    }

    if (quicconnect.request.closeID)
    {
      quic->forget(quicconnect.request.closeID);
      SetJSONResponse("OK", quicconnect.response);
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

    if (not check_endpoint_param(quiclistener))
      return;

    const auto& quic = m_Router.quic_tunnel();

    if (not quic)
    {
      SetJSONError(
          "No quic interface available on endpoint " + quiclistener.request.endpoint,
          quiclistener.response);
      return;
    }

    if (quiclistener.request.closeID)
    {
      quic->forget(quiclistener.request.closeID);
      SetJSONResponse("OK", quiclistener.response);
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
        SetJSONError(e.what(), quiclistener.response);
        return;
      }

      util::StatusObject result;
      result["id"] = id;
      result["addr"] = fmt::format(
          "{}:{}"_format(m_Router.get_layers()->flow->local_addr(), quiclistener.request.port));

      if (not quiclistener.request.srvProto.empty())
      {
        m_Router.get_layers()->platform->local_dns_zone().add_srv_record(
            std::make_tuple(quiclistener.request.srvProto, 1, 1, quiclistener.request.port, ""));
      }

      SetJSONResponse(result, quiclistener.response);
      return;
    }
  }

  void
  RPCServer::invoke(MapExit& mapexit)
  {
    MapExit exit_request;
    // steal replier from exit RPC endpoint.
    exit_request.replier.emplace(mapexit.move());
    // then do the actual call.
    m_Router.get_layers()->platform->map_exit(
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
    auto exit_list = json::array();
    for (const auto& exit : m_Router.get_layers()->platform->addr_mapper.all_exits())
      exit_list.emplace_back(to_json(exit));

    SetJSONResponse(exit_list.empty() ? "No exits" : exit_list, listexits.response);
  }

  void
  RPCServer::invoke(UnmapExit& unmapexit)
  {
    try
    {
      for (const auto& range : unmapexit.request.ip_range)
        m_Router.get_layers()->platform->unmap_all_exits_on_range(range);
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
    const auto& endpoint = m_Router.get_layers()->platform;
    auto current_exits = endpoint->addr_mapper.all_exits();

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

    layers::flow::FlowAddr old_exit{swapexits.request.exit_addresses[0]};
    // populate map_exit request with old IP ranges
    for (const auto& mapping : current_exits)
    {
      if (not mapping.flow_info)
        continue;

      if (mapping.flow_info->dst == old_exit)
      {
        map_request.request.ip_range = mapping.owned_ranges;
        unmap_request.request.ip_range = mapping.owned_ranges;
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
        [this, unmap = std::move(unmap_request), old_exit = std::move(old_exit)](
            bool success, std::string result) mutable {
          if (not success)
            unmap.send_response({{"error"}, std::move(result)});
          else
          {
            try
            {
              for (const auto& range : unmap.request.ip_range)
                m_Router.get_layers()->platform->unmap_exit(old_exit, range);
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
    dns::RRType qtype = dns::get_rr_type(dnsquery.request.qtype).value_or(dns::RRType::A);

    dns::Message msg{};
    msg.questions.emplace_back(split(qname, "."), qtype);

    if (not check_endpoint_param(dnsquery))
      return;

    if (const auto& dns = m_Router.get_dns())
    {
      auto packet_src = std::make_shared<DummyPacketSource>(
          [&](auto result) { SetJSONResponse(result.to_json(), dnsquery.response); });
      try
      {
        if (not dns->MaybeHandlePacket(
                packet_src, packet_src->dumb, packet_src->dumb, msg.ToBuffer()))
          SetJSONError("DNS query not accepted by endpoint", dnsquery.response);
      }
      catch (std::exception& ex)
      {
        SetJSONError("Failed to handle dns packet: {}"_format(ex.what()), dnsquery.response);
      }
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

        if (parser.LoadNewFromStr(config.request.ini))
        {
          parser.Filename(conf_d / (config.request.filename));
          parser.SaveNew();
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
