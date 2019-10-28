#include <rpc/rpc.hpp>

#include <constants/version.hpp>
#include <router/abstractrouter.hpp>
#include <service/context.hpp>
#include <util/logging/logger.hpp>
#include <router_id.hpp>
#include <exit/context.hpp>

#include <util/encode.hpp>
#include <util/meta/memfn.hpp>
#include <utility>

namespace llarp
{
  namespace rpc
  {

    struct CallerImpl
    {
      /*
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
      */
    };


    struct ServerImpl
    {
      /*
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
      */
    };

    Caller::Caller(AbstractRouter* r)
        // : m_Impl(std::make_unique< CallerImpl >(r))
    {
    }

    Caller::~Caller() = default;

    void
    Caller::Stop()
    {
      // m_Impl->Stop();
    }

    bool
    Caller::Start(const std::string& addr)
    {
      return false; // TODO:
      // return m_Impl->Start(addr);
    }

    void
    Caller::Tick(llarp_time_t now)
    {
      // m_Impl->Tick(now);
    }

    void
    Caller::SetAuth(const std::string& user, const std::string& passwd)
    {
      // m_Impl->SetAuth(user, passwd);
    }

    Server::Server(AbstractRouter* r)
        // : m_Impl(std::make_unique< ServerImpl >(r))
    {
    }

    Server::~Server() = default;

    void
    Server::Stop()
    {
      // m_Impl->Stop();
    }

    bool
    Server::Start(const std::string& addr)
    {
      return false; // TODO
      // return m_Impl->Start(addr);
    }

  }  // namespace rpc
}  // namespace llarp
