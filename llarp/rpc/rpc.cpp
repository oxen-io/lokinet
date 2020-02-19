#include <rpc/rpc.hpp>

#include <constants/version.hpp>
#include <router/abstractrouter.hpp>
#include <service/context.hpp>
#include <util/logging/logger.hpp>
#include <router_id.hpp>
#include <exit/context.hpp>

#include <util/encode.hpp>
#include <util/meta/memfn.hpp>
#include <libabyss.hpp>
#include <utility>

namespace llarp
{
  namespace rpc
  {
    struct CallerHandler : public ::abyss::http::IRPCClientHandler
    {
      CallerImpl* m_Parent;
      CallerHandler(::abyss::http::ConnImpl* impl, CallerImpl* parent)
          : ::abyss::http::IRPCClientHandler(impl), m_Parent(parent)
      {
      }

      ~CallerHandler() override = default;

      virtual bool
      HandleJSONResult(const nlohmann::json& val) = 0;

      bool
      HandleResponse(::abyss::http::RPC_Response response) override
      {
        if(!response.is_object())
        {
          return HandleJSONResult({});
        }
        const auto itr = response.find("result");
        if(itr == response.end())
        {
          return HandleJSONResult({});
        }
        if(itr.value().is_object())
        {
          return HandleJSONResult(itr.value());
        }

        return false;
      }

      void
      PopulateReqHeaders(abyss::http::Headers_t& hdr) override;
    };

    struct LokiPingHandler final : public CallerHandler
    {
      ~LokiPingHandler() override = default;
      LokiPingHandler(::abyss::http::ConnImpl* impl, CallerImpl* parent)
          : CallerHandler(impl, parent)
      {
      }
      bool
      HandleJSONResult(const nlohmann::json& result) override
      {
        if(not result.is_object())
        {
          LogError("invalid result from lokid ping, not an object");
          return false;
        }
        const auto itr = result.find("status");
        if(itr == result.end())
        {
          LogError("invalid result from lokid ping, no result");
          return false;
        }
        if(not itr->is_string())
        {
          LogError("invalid result from lokid ping, status not an string");
          return false;
        }
        const auto status = itr->get< std::string >();
        if(status != "OK")
        {
          LogError("lokid ping failed: '", status, "'");
          return false;
        }
        LogInfo("lokid ping: '", status, "'");
        return true;
      }
      void
      HandleError() override
      {
        LogError("Failed to ping lokid");
      }
    };

    struct GetServiceNodeListHandler final : public CallerHandler
    {
      using PubkeyList_t = std::vector< RouterID >;
      using Callback_t   = std::function< void(const PubkeyList_t&, bool) >;

      ~GetServiceNodeListHandler() override = default;
      Callback_t handler;

      GetServiceNodeListHandler(::abyss::http::ConnImpl* impl,
                                CallerImpl* parent, Callback_t h)
          : CallerHandler(impl, parent), handler(std::move(h))
      {
      }

      bool
      HandleJSONResult(const nlohmann::json& result) override;

      void
      HandleError() override
      {
        handler({}, false);
      }
    };

    struct CallerImpl : public ::abyss::http::JSONRPC
    {
      AbstractRouter* router;
      llarp_time_t m_NextKeyUpdate = 0s;
      std::string m_LastBlockHash;
      llarp_time_t m_NextPing              = 0s;
      const llarp_time_t KeyUpdateInterval = 5s;
      const llarp_time_t PingInterval      = 5min;
      using PubkeyList_t = GetServiceNodeListHandler::PubkeyList_t;

      CallerImpl(AbstractRouter* r) : ::abyss::http::JSONRPC(), router(r)
      {
      }

      void
      Tick(llarp_time_t now)
      {
        if(not router->IsRunning())
          return;
        if(not router->IsServiceNode())
          return;
        if(now >= m_NextKeyUpdate)
        {
          AsyncUpdatePubkeyList();
          m_NextKeyUpdate = now + KeyUpdateInterval;
        }
        if(now >= m_NextPing)
        {
          AsyncLokiPing();
          m_NextPing = now + PingInterval;
        }
        Flush();
      }

      void
      SetAuth(const std::string& user, const std::string& passwd)
      {
        username = user;
        password = passwd;
      }

      void
      AsyncLokiPing()
      {
        LogInfo("Pinging Lokid");
        nlohmann::json version(llarp::VERSION);
        nlohmann::json params({{"version", version}});
        QueueRPC("lokinet_ping", std::move(params),
                 util::memFn(&CallerImpl::NewLokinetPingConn, this));
      }

      void
      AsyncUpdatePubkeyList()
      {
        LogDebug("Updating service node list");
        nlohmann::json params = {{"fields",
                                  {
                                      {"pubkey_ed25519", true},
                                      {"active", true},
                                      {"funded", true},
                                      {"block_hash", true},
                                  }},
                                 {"poll_block_hash", m_LastBlockHash}};
        QueueRPC("get_n_service_nodes", std::move(params),
                 util::memFn(&CallerImpl::NewAsyncUpdatePubkeyListConn, this));
      }

      bool
      Start(const std::string& remote)
      {
        return RunAsync(router->netloop(), remote);
      }

      abyss::http::IRPCClientHandler*
      NewLokinetPingConn(abyss::http::ConnImpl* impl)
      {
        return new LokiPingHandler(impl, this);
      }

      abyss::http::IRPCClientHandler*
      NewAsyncUpdatePubkeyListConn(abyss::http::ConnImpl* impl)
      {
        return new GetServiceNodeListHandler(
            impl, this,
            util::memFn(&CallerImpl::HandleServiceNodeListUpdated, this));
      }

      void
      HandleServiceNodeListUpdated(const PubkeyList_t& list, bool updated)
      {
        if(updated)
        {
          router->SetRouterWhitelist(list);
        }
        else
          LogError("service node list not updated");
      }

      ~CallerImpl() = default;
    };

    bool
    GetServiceNodeListHandler::HandleJSONResult(const nlohmann::json& result)
    {
      PubkeyList_t keys;
      if(not result.is_object())
      {
        LogWarn("Invalid result: not an object");
        handler({}, false);
        return false;
      }
      // If lokid says tells us the block didn't change then nothing to do
      const auto unchanged_it = result.find("unchanged");
      if(unchanged_it != result.end() and unchanged_it->get< bool >())
        return true;

      const auto hash_it = result.find("block_hash");
      if(hash_it == result.end())
      {
        LogWarn("Invalid result: no block_hash member");
        handler({}, false);
        return false;
      }
      else
        m_Parent->m_LastBlockHash = hash_it->get< std::string >();

      const auto itr = result.find("service_node_states");
      if(itr == result.end())
      {
        LogWarn("Invalid result: no service_node_states member");
        handler({}, false);
        return false;
      }
      if(not itr.value().is_array())
      {
        LogWarn("Invalid result: service_node_states is not an array");
        handler({}, false);
        return false;
      }
      for(const auto item : itr.value())
      {
        if(not item.is_object())
          continue;
        if(not item.value("active", false))
          continue;
        if(not item.value("funded", false))
          continue;
        const std::string pk = item.value("pubkey_ed25519", "");
        if(pk.empty())
          continue;
        PubKey k;
        if(k.FromString(pk))
          keys.emplace_back(std::move(k));
      }
      handler(keys, not keys.empty());
      return true;
    }

    void
    CallerHandler::PopulateReqHeaders(abyss::http::Headers_t& hdr)
    {
      hdr.emplace("User-Agent", "lokinet rpc (YOLO)");
    }

    struct Handler : public ::abyss::httpd::IRPCHandler
    {
      using json = nlohmann::json;
      AbstractRouter* router;
      std::unordered_map< std::string, std::function< json(json) > > m_dispatch;
      Handler(::abyss::httpd::ConnImpl* conn, AbstractRouter* r)
          : ::abyss::httpd::IRPCHandler(conn)
          , router(r)
          , m_dispatch{
                {"llarp.admin.wakeup", [=](auto&&) { return StartRouter(); }},
                {"llarp.admin.link.neighbor",
                 [=](auto&&) { return ListNeighbors(); }},
                {"llarp.admin.exit.list",
                 [=](auto&&) { return ListExitLevels(); }},
                {"llarp.admin.dumpstate", [=](auto&&) { return DumpState(); }},
                {"llarp.admin.status", [=](auto&&) { return DumpStatus(); }},
                {"llarp.our.addresses", [=](auto&&) { return OurAddresses(); }},
                {"llarp.version", [=](auto&&) { return DumpVersion(); }}}
      {
      }

      ~Handler() override = default;

      json
      StartRouter() const
      {
        const bool rc = router->Run();
        return {{"status", rc}};
      }

      json
      DumpState() const
      {
        return router->ExtractStatus();
      }

      json
      ListExitLevels() const
      {
        exit::Context::TrafficStats stats;
        router->exitContext().CalculateExitTraffic(stats);
        json resp;

        for(const auto& stat : stats)
        {
          resp.push_back({
              {"ident", stat.first.ToHex()},
              {"tx", stat.second.first},
              {"rx", stat.second.second},
          });
        }
        return resp;
      }

      json
      ListNeighbors() const
      {
        json resp = json::array();
        router->ForEachPeer(
            [&](const ILinkSession* session, bool outbound) {
              resp.push_back(
                  {{"ident", RouterID(session->GetPubKey()).ToString()},
                   {"svcnode", session->GetRemoteRC().IsPublicRouter()},
                   {"outbound", outbound}});
            },
            false);
        return resp;
      }

      json
      DumpStatus() const
      {
        size_t numServices      = 0;
        size_t numServicesReady = 0;
        json services           = json::array();
        auto visitor =
            [&](const std::string& name,
                const std::shared_ptr< service::Endpoint >& ptr) -> bool {
          numServices++;
          if(ptr->IsReady())
            numServicesReady++;
          services.push_back({name,
                              json{{"ready", ptr->IsReady()},
                                   {"stopped", ptr->IsStopped()},
                                   {"stale", ptr->IntrosetIsStale()}}});
          return true;
        };
        router->hiddenServiceContext().ForEachService(visitor);
        return {{"uptime", to_json(router->Uptime())},
                {"servicesTotal", numServices},
                {"servicesReady", numServicesReady},
                {"services", services}};
      }

      json
      OurAddresses() const
      {
        json services;
        router->hiddenServiceContext().ForEachService(
            [&](const std::string&,
                const std::shared_ptr< service::Endpoint >& service) {
              const service::Address addr = service->GetIdentity().pub.Addr();
              services.push_back(addr.ToString());
              return true;
            });

        return {{"services", services}};
      }

      json
      DumpVersion() const
      {
        return {{"version", llarp::VERSION_FULL}};
      }

      json
      HandleJSONRPC(std::string method, const json& params) override
      {
        auto it = m_dispatch.find(method);
        if(it != m_dispatch.end())
        {
          return it->second(params);
        }
        return false;
      }
    };

    struct ReqHandlerImpl : public ::abyss::httpd::BaseReqHandler
    {
      ReqHandlerImpl(AbstractRouter* r, llarp_time_t reqtimeout)
          : ::abyss::httpd::BaseReqHandler(reqtimeout), router(r)
      {
      }
      AbstractRouter* router;
      ::abyss::httpd::IRPCHandler*
      CreateHandler(::abyss::httpd::ConnImpl* conn) override
      {
        return new Handler(conn, router);
      }
    };

    struct ServerImpl
    {
      AbstractRouter* router;
      ReqHandlerImpl _handler;

      ServerImpl(AbstractRouter* r) : router(r), _handler(r, 2s)
      {
      }

      ~ServerImpl() = default;

      void
      Stop()
      {
        _handler.Close();
      }

      bool
      Start(const std::string& addr)
      {
        sockaddr_in saddr;
        saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        saddr.sin_family      = AF_INET;
        saddr.sin_port        = 0;

        auto idx = addr.find_first_of(':');
        if(idx != std::string::npos)
        {
          Addr netaddr{addr.substr(0, idx), addr.substr(1 + idx)};
          saddr.sin_addr.s_addr = netaddr.ton();
          saddr.sin_port        = htons(netaddr.port());
        }
        return _handler.ServeAsync(router->netloop(), router->logic(),
                                   (const sockaddr*)&saddr);
      }
    };

    Caller::Caller(AbstractRouter* r)
        : m_Impl(std::make_unique< CallerImpl >(r))
    {
    }

    Caller::~Caller() = default;

    void
    Caller::Stop()
    {
      m_Impl->Stop();
    }

    bool
    Caller::Start(const std::string& addr)
    {
      return m_Impl->Start(addr);
    }

    void
    Caller::Tick(llarp_time_t now)
    {
      m_Impl->Tick(now);
    }

    void
    Caller::SetAuth(const std::string& user, const std::string& passwd)
    {
      m_Impl->SetAuth(user, passwd);
    }

    Server::Server(AbstractRouter* r)
        : m_Impl(std::make_unique< ServerImpl >(r))
    {
    }

    Server::~Server() = default;

    void
    Server::Stop()
    {
      m_Impl->Stop();
    }

    bool
    Server::Start(const std::string& addr)
    {
      return m_Impl->Start(addr);
    }

  }  // namespace rpc
}  // namespace llarp
