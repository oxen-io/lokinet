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

      void
      PopulateReqHeaders(__attribute__((unused)) abyss::http::Headers_t& hdr)
      {
      }
    };

    struct VerifyRouterHandler : public CallerHandler
    {
      llarp::PubKey pk;
      std::function< void(llarp::PubKey, bool) > handler;

      ~VerifyRouterHandler()
      {
      }

      VerifyRouterHandler(::abyss::http::ConnImpl* impl, const llarp::PubKey& k,
                          std::function< void(llarp::PubKey, bool) > h)
          : CallerHandler(impl), pk(k), handler(h)
      {
      }

      bool
      HandleResponse(__attribute__((unused))
                     const ::abyss::http::RPC_Response& response)
      {
        handler(pk, true);
        return true;
      }

      void
      HandleError()
      {
        llarp::LogInfo("failed to verify router ", pk);
        handler(pk, false);
      }
    };

    struct CallerImpl : public ::abyss::http::JSONRPC
    {
      llarp_router* router;
      CallerImpl(llarp_router* r) : ::abyss::http::JSONRPC(), router(r)
      {
      }

      void
      Tick()
      {
        Flush();
      }

      bool
      Start(const std::string& remote)
      {
        return RunAsync(router->netloop, remote);
      }

      abyss::http::IRPCClientHandler*
      NewConn(PubKey k, std::function< void(llarp::PubKey, bool) > handler,
              abyss::http::ConnImpl* impl)
      {
        return new VerifyRouterHandler(impl, k, handler);
      }

      void
      AsyncVerifyRouter(llarp::PubKey pk,
                        std::function< void(llarp::PubKey, bool) > handler)
      {
        abyss::json::Value params;
        params.SetObject();
        QueueRPC("get_service_node", std::move(params),
                 std::bind(&CallerImpl::NewConn, this, pk, handler,
                           std::placeholders::_1));
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
      Tick()
      {
      }

      void
      AsyncVerifyRouter(llarp::PubKey pk,
                        std::function< void(llarp::PubKey, bool) > result)
      {
        // always allow routers when not using libabyss
        result(pk, true);
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
    Caller::AsyncVerifyRouter(
        llarp::PubKey pk, std::function< void(llarp::PubKey, bool) > handler)
    {
      m_Impl->AsyncVerifyRouter(pk, handler);
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
