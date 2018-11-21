#include <llarp/rpc.hpp>

#include "router.hpp"
#ifdef USE_ABYSS
#include <libabyss.hpp>
#endif
namespace llarp
{
  namespace rpc
  {
#ifdef USE_ABYSS

    struct CallerHandler : public ::abyss::http::IRPCClientHandler
    {
      CallerHandler(::abyss::http::ConnImpl* impl)
          : ::abyss::http::IRPCClientHandler(impl)
      {
      }

      ~CallerHandler()
      {
      }

      virtual bool
      HandleJSONResult(const ::abyss::json::Value& val) = 0;

      bool
      HandleResponse(::abyss::http::RPC_Response response)
      {
        if(!response.IsObject())
        {
          return HandleJSONResult({});
        }
        const auto itr = response.FindMember("result");
        if(itr == response.MemberEnd())
        {
          return HandleJSONResult({});
        }
        if(itr->value.IsObject())
          return HandleJSONResult(itr->value);
        return false;
      }

      void
      PopulateReqHeaders(abyss::http::Headers_t& hdr)
      {
        (void)hdr;
        // TODO: add http auth (?)
      }
    };

    struct GetServiceNodeListHandler final : public CallerHandler
    {
      using PubkeyList_t = std::vector< llarp::PubKey >;
      using Callback_t   = std::function< void(const PubkeyList_t&, bool) >;

      ~GetServiceNodeListHandler()
      {
      }
      Callback_t handler;

      GetServiceNodeListHandler(::abyss::http::ConnImpl* impl, Callback_t h)
          : CallerHandler(impl), handler(h)
      {
      }

      bool
      HandleJSONResult(const ::abyss::json::Value& result) override
      {
        PubkeyList_t keys;
        if(!result.IsObject())
        {
          handler({}, false);
          return false;
        }
        const auto itr = result.FindMember("keys");
        if(itr == result.MemberEnd())
        {
          handler({}, false);
          return false;
        }
        if(!itr->value.IsArray())
        {
          handler({}, false);
          return false;
        }
        auto key_itr = itr->value.Begin();
        while(key_itr != itr->value.End())
        {
          if(key_itr->IsString())
          {
            keys.emplace_back();
            if(!HexDecode(key_itr->GetString(), keys.back(), PUBKEYSIZE))
            {
              keys.pop_back();
            }
          }
          ++key_itr;
        }
        handler(keys, true);
        return true;
      }

      void
      HandleError()
      {
        handler({}, false);
      }
    };

    struct CallerImpl : public ::abyss::http::JSONRPC
    {
      llarp_router* router;
      llarp_time_t m_NextKeyUpdate;
      const llarp_time_t KeyUpdateInterval = 1000 * 60 * 2;
      using PubkeyList_t = GetServiceNodeListHandler::PubkeyList_t;

      CallerImpl(llarp_router* r) : ::abyss::http::JSONRPC(), router(r)
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
      AsyncUpdatePubkeyList()
      {
        llarp::LogInfo("Updating service node list");
        ::abyss::json::Value params;
        params.SetObject();
        QueueRPC("/get_all_service_node_keys", std::move(params),
                 std::bind(&CallerImpl::NewAsyncUpdatePubkeyListConn, this,
                           std::placeholders::_1));
      }

      bool
      Start(const std::string& remote)
      {
        return RunAsync(router->netloop, remote);
      }

      abyss::http::IRPCClientHandler*
      NewAsyncUpdatePubkeyListConn(abyss::http::ConnImpl* impl)
      {
        return new GetServiceNodeListHandler(
            impl,
            std::bind(&CallerImpl::HandleServiceNodeListUpdated, this,
                      std::placeholders::_1, std::placeholders::_2));
      }

      void
      HandleServiceNodeListUpdated(const PubkeyList_t& list, bool updated)
      {
        if(updated)
        {
          router->lokinetRouters.clear();
          for(const auto& pk : list)
            router->lokinetRouters.insert(
                std::make_pair(pk, std::numeric_limits< llarp_time_t >::max()));
          llarp::LogInfo("updated service node list, we have ",
                         router->lokinetRouters.size(), " authorized routers");
        }
        else
          llarp::LogError("service node list not updated");
      }

      ~CallerImpl()
      {
      }
    };

    struct Handler : public ::abyss::httpd::IRPCHandler
    {
      llarp_router* router;
      Handler(::abyss::httpd::ConnImpl* conn, llarp_router* r)
          : ::abyss::httpd::IRPCHandler(conn), router(r)
      {
      }

      ~Handler()
      {
      }

      bool
      ListExitLevels(Response& resp) const
      {
        llarp::exit::Context::TrafficStats stats;
        router->exitContext.CalculateExitTraffic(stats);
        auto& alloc = resp.GetAllocator();
        abyss::json::Value exits;
        exits.SetArray();
        auto itr = stats.begin();
        while(itr != stats.end())
        {
          abyss::json::Value info, ident;
          info.SetObject();
          ident.SetString(itr->first.ToHex().c_str(), alloc);
          info.AddMember("ident", ident, alloc);
          info.AddMember("tx", abyss::json::Value(itr->second.first), alloc);
          info.AddMember("rx", abyss::json::Value(itr->second.second), alloc);
          exits.PushBack(info, alloc);
          ++itr;
        }
        resp.AddMember("result", exits, alloc);
        return true;
      }

      bool
      ListNeighboors(Response& resp) const
      {
        auto& alloc = resp.GetAllocator();
        abyss::json::Value peers;
        peers.SetArray();
        router->ForEachPeer(
            [&](const llarp::ILinkSession* session, bool outbound) {
              abyss::json::Value peer;
              peer.SetObject();
              abyss::json::Value ident_val, addr_val;

              auto ident = session->GetPubKey().ToHex();
              ident_val.SetString(ident.c_str(), alloc);

              auto addr = session->GetRemoteEndpoint().ToString();
              addr_val.SetString(addr.c_str(), alloc);

              peer.AddMember("addr", addr_val, alloc);
              peer.AddMember("ident", ident_val, alloc);
              peer.AddMember("outbound", abyss::json::Value(outbound), alloc);
              peers.PushBack(peer, alloc);
            });
        resp.AddMember("result", peers, alloc);
        return true;
      }

      bool
      HandleJSONRPC(Method_t method,
                    __attribute__((unused)) const Params& params,
                    Response& response)
      {
        if(method == "llarp.admin.link.neighboors")
        {
          return ListNeighboors(response);
        }
        else if(method == "llarp.admin.exit.list")
        {
          return ListExitLevels(response);
        }
        return false;
      }
    };

    struct ReqHandlerImpl : public ::abyss::httpd::BaseReqHandler
    {
      ReqHandlerImpl(llarp_router* r, llarp_time_t reqtimeout)
          : ::abyss::httpd::BaseReqHandler(reqtimeout), router(r)
      {
      }
      llarp_router* router;
      ::abyss::httpd::IRPCHandler*
      CreateHandler(::abyss::httpd::ConnImpl* conn)
      {
        return new Handler(conn, router);
      }
    };

    struct ServerImpl
    {
      llarp_router* router;
      ReqHandlerImpl _handler;

      ServerImpl(llarp_router* r) : router(r), _handler(r, 2000)
      {
      }

      ~ServerImpl()
      {
      }

      bool
      Start(const std::string& addr)
      {
        uint16_t port = 0;
        auto idx      = addr.find_first_of(':');
        llarp::Addr netaddr;
        if(idx != std::string::npos)
        {
          port    = std::stoi(addr.substr(1 + idx));
          netaddr = llarp::Addr(addr.substr(0, idx));
        }
        sockaddr_in saddr;
        saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        saddr.sin_family      = AF_INET;
        saddr.sin_port        = htons(port);
        return _handler.ServeAsync(router->netloop, router->logic,
                                   (const sockaddr*)&saddr);
      }
    };
#else
    struct ServerImpl
    {
      ServerImpl(__attribute__((unused)) llarp_router* r){};

      bool
      Start(__attribute__((unused)) const std::string& addr)
      {
        return true;
      }
    };

    struct CallerImpl
    {
      CallerImpl(__attribute__((unused)) llarp_router* r)
      {
      }

      ~CallerImpl()
      {
      }

      bool
      Start(const std::string&)
      {
        return true;
      }

      void
      Tick(llarp_time_t now)
      {
        (void)now;
      }
    };

#endif

    Caller::Caller(llarp_router* r) : m_Impl(new CallerImpl(r))
    {
    }

    Caller::~Caller()
    {
      delete m_Impl;
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

    Server::Server(llarp_router* r) : m_Impl(new ServerImpl(r))
    {
    }

    Server::~Server()
    {
      delete m_Impl;
    }

    bool
    Server::Start(const std::string& addr)
    {
      return m_Impl->Start(addr);
    }

  }  // namespace rpc
}  // namespace llarp
