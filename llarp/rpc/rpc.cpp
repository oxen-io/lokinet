#include <rpc/rpc.hpp>

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
      HandleJSONResult(const nlohmann::json& result) override
      {
        PubkeyList_t keys;
        if(!result.is_object())
        {
          LogWarn("Invalid result: not an object");
          handler({}, false);
          return false;
        }
        const auto itr = result.find("keys");
        if(itr == result.end())
        {
          LogWarn("Invalid result: no keys member");
          handler({}, false);
          return false;
        }
        if(!itr.value().is_array())
        {
          LogWarn("Invalid result: keys is not an array");
          handler({}, false);
          return false;
        }
        for(const auto item : itr.value())
        {
          if(item.is_string())
          {
            keys.emplace_back();
            std::string str = item.get< std::string >();
            if(!Base32Decode(str, keys.back()))
            {
              LogWarn("Invalid key: ", str);
              keys.pop_back();
            }
          }
        }
        handler(keys, true);
        return true;
      }

      void
      HandleError() override
      {
        handler({}, false);
      }
    };

    struct CallerImpl : public ::abyss::http::JSONRPC
    {
      AbstractRouter* router;
      llarp_time_t m_NextKeyUpdate         = 0;
      const llarp_time_t KeyUpdateInterval = 5000;
      using PubkeyList_t = GetServiceNodeListHandler::PubkeyList_t;

      CallerImpl(AbstractRouter* r) : ::abyss::http::JSONRPC(), router(r)
      {
      }

      void
      Tick(llarp_time_t now)
      {
        if(now >= m_NextKeyUpdate)
        {
          AsyncUpdatePubkeyList();
          m_NextKeyUpdate = now + KeyUpdateInterval;
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
      AsyncUpdatePubkeyList()
      {
        LogInfo("Updating service node list");
        QueueRPC("get_all_service_nodes_keys", nlohmann::json::object(),
                 util::memFn(&CallerImpl::NewAsyncUpdatePubkeyListConn, this));
      }

      bool
      Start(const std::string& remote)
      {
        return RunAsync(router->netloop(), remote);
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

    void
    CallerHandler::PopulateReqHeaders(abyss::http::Headers_t& hdr)
    {
      hdr.emplace("User-Agent", "lokinet rpc (YOLO)");
    }

    struct Handler : public ::abyss::httpd::IRPCHandler
    {
      AbstractRouter* router;
      Handler(::abyss::httpd::ConnImpl* conn, AbstractRouter* r)
          : ::abyss::httpd::IRPCHandler(conn), router(r)
      {
      }

      ~Handler() override = default;

      Response
      DumpState() const
      {
        return router->ExtractStatus();
      }

      Response
      ListExitLevels() const
      {
        exit::Context::TrafficStats stats;
        router->exitContext().CalculateExitTraffic(stats);
        Response resp;

        for(const auto& stat : stats)
        {
          resp.emplace_back(Response{
              {"ident", stat.first.ToHex()},
              {"tx", stat.second.first},
              {"rx", stat.second.second},
          });
        }
        return resp;
      }

      Response
      ListNeighboors() const
      {
        Response resp = Response::array();
        router->ForEachPeer(
            [&](const ILinkSession* session, bool outbound) {
              resp.emplace_back(
                  Response{{"ident", RouterID(session->GetPubKey()).ToString()},
                           {"svcnode", session->GetRemoteRC().IsPublicRouter()},
                           {"outbound", outbound}});
            },
            false);
        return resp;
      }

      Response
      DumpStatus() const
      {
        size_t numServices      = 0;
        size_t numServicesReady = 0;
        Response services       = Response::array();
        auto visitor =
            [&](const std::string& name,
                const std::shared_ptr< service::Endpoint >& ptr) -> bool {
          numServices++;
          if(ptr->IsReady())
            numServicesReady++;
          const Response status{{"ready", ptr->IsReady()},
                                {"stopped", ptr->IsStopped()},
                                {"stale", ptr->IntrosetIsStale()}};
          services.emplace_back(Response{name, status});
          return true;
        };
        router->hiddenServiceContext().ForEachService(visitor);
        const Response resp{{"uptime", router->Uptime()},
                            {"servicesTotal", numServices},
                            {"servicesReady", numServicesReady},
                            {"services", services}};
        return resp;
      }

      absl::optional< Response >
      HandleJSONRPC(Method_t method,
                    ABSL_ATTRIBUTE_UNUSED const Params& params) override
      {
        if(method == "llarp.admin.link.neighboors")
        {
          return ListNeighboors();
        }
        if(method == "llarp.admin.exit.list")
        {
          return ListExitLevels();
        }
        if(method == "llarp.admin.dumpstate")
        {
          return DumpState();
        }
        if(method == "llarp.admin.status")
        {
          return DumpStatus();
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

      ServerImpl(AbstractRouter* r) : router(r), _handler(r, 2000)
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
        uint16_t port = 0;
        auto idx      = addr.find_first_of(':');
        Addr netaddr;
        if(idx != std::string::npos)
        {
          port    = std::stoi(addr.substr(1 + idx));
          netaddr = Addr(addr.substr(0, idx));
        }
        sockaddr_in saddr;
        saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        saddr.sin_family      = AF_INET;
        saddr.sin_port        = htons(port);
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
