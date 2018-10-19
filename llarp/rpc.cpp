#include <llarp/rpc.hpp>
#include <libabyss.hpp>

namespace llarp
{
  namespace rpc
  {
    struct ServerImpl : public ::abyss::http::BaseReqHandler
    {
      llarp_router* router;

      ServerImpl(llarp_router* r)
          : ::abyss::http::BaseReqHandler(2000), router(r)
      {
      }

      bool
      Start(const std::string& addr)
      {
        return false;
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
