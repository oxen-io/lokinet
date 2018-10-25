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
      HandleJSONRPC(Method_t method, Params params, Response& response)
      {
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
