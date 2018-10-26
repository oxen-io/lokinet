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
      HandleJSONRPC(Method_t method, const Params& params, Response& response)
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
