#include <llarp/rpc.hpp>
#include <libabyss.hpp>

#include "router.hpp"

namespace llarp
{
  namespace rpc
  {
    struct Handler : public ::abyss::http::IRPCHandler
    {
      llarp_router* router;
      Handler(::abyss::http::ConnImpl* conn, llarp_router* r)
          : ::abyss::http::IRPCHandler(conn), router(r)
      {
      }

      ~Handler()
      {
      }

      bool
      ListNeighboors(Response& resp) const
      {
        auto& alloc = resp.GetAllocator();
        auto& peers = abyss::json::Value().SetArray();
        router->ForEachPeer([&](const llarp::ILinkSession* session,
                                bool outbound) {
          auto& peer = abyss::json::Value().SetObject();
          auto ident = session->GetPubKey().ToHex();
          auto addr  = session->GetRemoteEndpoint().ToString();
          peer.AddMember("addr",
                         abyss::json::Value(addr.data(), addr.size(), alloc),
                         alloc);
          peer.AddMember("ident",
                         abyss::json::Value(ident.data(), ident.size(), alloc),
                         alloc);
          peer.AddMember("outbound", abyss::json::Value(outbound), alloc);
          peers.PushBack(peer, alloc);
        });
        resp.AddMember("result", peers, alloc);
        return true;
      }

      bool
      HandleJSONRPC(Method_t method, Params params, Response& response)
      {
        if(method == "llarp.admin.link.neighboors")
        {
          return ListNeighboors(response);
        }
        return false;
      }
    };

    struct ReqHandlerImpl : public ::abyss::http::BaseReqHandler
    {
      ReqHandlerImpl(llarp_router* r, llarp_time_t reqtimeout)
          : ::abyss::http::BaseReqHandler(reqtimeout), router(r)
      {
      }
      llarp_router* router;
      ::abyss::http::IRPCHandler*
      CreateHandler(::abyss::http::ConnImpl* conn) const
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
        llarp::Addr bindaddr(addr);
        return _handler.ServeAsync(router->netloop, router->logic, bindaddr);
      }
    };

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
