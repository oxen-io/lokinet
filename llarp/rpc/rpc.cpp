#include <rpc/rpc.hpp>

#include <router/abstractrouter.hpp>
#include <util/logger.hpp>
#include <router_id.hpp>
#include <exit/context.hpp>

#include <util/encode.hpp>
#include <libabyss.hpp>

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

      ~CallerHandler()
      {
      }

      virtual bool
      HandleJSONResult(const json::Value& val) = 0;

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
      PopulateReqHeaders(abyss::http::Headers_t& hdr);
    };

    struct GetServiceNodeListHandler final : public CallerHandler
    {
      using PubkeyList_t = std::vector< RouterID >;
      using Callback_t   = std::function< void(const PubkeyList_t&, bool) >;

      ~GetServiceNodeListHandler()
      {
      }
      Callback_t handler;

      GetServiceNodeListHandler(::abyss::http::ConnImpl* impl,
                                CallerImpl* parent, Callback_t h)
          : CallerHandler(impl, parent), handler(h)
      {
      }

      bool
      HandleJSONResult(const json::Value& result) override
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
            std::string str = key_itr->GetString();
            if(str.size() != Base32DecodeSize(keys.back().size()))
            {
              keys.pop_back();
            }
            else if(!Base32Decode(str, keys.back()))
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
      HandleError() override
      {
        handler({}, false);
      }
    };

    struct CallerImpl : public ::abyss::http::JSONRPC
    {
      AbstractRouter* router;
      llarp_time_t m_NextKeyUpdate         = 0;
      const llarp_time_t KeyUpdateInterval = 1000 * 60 * 2;
      using PubkeyList_t = GetServiceNodeListHandler::PubkeyList_t;

      std::string username;
      std::string password;

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
      SetBasicAuth(const std::string& user, const std::string& passwd)
      {
        username = user;
        password = passwd;
      }

      void
      AsyncUpdatePubkeyList()
      {
        LogInfo("Updating service node list");
        json::Value params;
        params.SetObject();
        QueueRPC("get_all_service_nodes_keys", std::move(params),
                 std::bind(&CallerImpl::NewAsyncUpdatePubkeyListConn, this,
                           std::placeholders::_1));
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
            std::bind(&CallerImpl::HandleServiceNodeListUpdated, this,
                      std::placeholders::_1, std::placeholders::_2));
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

      ~CallerImpl()
      {
      }
    };

    void
    CallerHandler::PopulateReqHeaders(abyss::http::Headers_t& hdr)
    {
      if(m_Parent->username.empty() || m_Parent->password.empty())
        return;
      std::stringstream ss;
      ss << "Basic ";
      std::string cred = m_Parent->username + ":" + m_Parent->password;
      llarp::Base64Encode(ss, (const byte_t*)cred.c_str(), cred.size());
      hdr.emplace("Authorization", ss.str());
    }

    struct Handler : public ::abyss::httpd::IRPCHandler
    {
      AbstractRouter* router;
      Handler(::abyss::httpd::ConnImpl* conn, AbstractRouter* r)
          : ::abyss::httpd::IRPCHandler(conn), router(r)
      {
      }

      ~Handler()
      {
      }

      bool
      DumpState(Response& resp) const
      {
        util::StatusObject dump = router->ExtractStatus();
        dump.Impl.Accept(resp);
        return true;
      }

      bool
      ListExitLevels(Response& resp) const
      {
        exit::Context::TrafficStats stats;
        router->exitContext().CalculateExitTraffic(stats);
        resp.StartArray();
        auto itr = stats.begin();
        while(itr != stats.end())
        {
          resp.StartObject();
          resp.Key("ident");
          resp.String(itr->first.ToHex().c_str());
          resp.Key("tx");
          resp.Uint64(itr->second.first);
          resp.Key("rx");
          resp.Uint64(itr->second.second);
          resp.EndObject();
          ++itr;
        }
        resp.EndArray();
        return true;
      }

      bool
      ListNeighboors(Response& resp) const
      {
        resp.StartArray();
        router->ForEachPeer([&](const ILinkSession* session, bool outbound) {
          resp.StartObject();
          auto ident = RouterID(session->GetPubKey()).ToString();
          resp.Key("ident");
          resp.String(ident.c_str());

          auto addr = session->GetRemoteEndpoint().ToString();
          resp.Key("addr");
          resp.String(addr.c_str());

          resp.Key("outbound");
          resp.Bool(outbound);
          resp.EndObject();
        });
        resp.EndArray();
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
        else if(method == "llarp.admin.dumpstate")
        {
          return DumpState(response);
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
      CreateHandler(::abyss::httpd::ConnImpl* conn)
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

      ~ServerImpl()
      {
      }

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

    Caller::~Caller()
    {
    }

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
    Caller::SetBasicAuth(const std::string& user, const std::string& passwd)
    {
      m_Impl->SetBasicAuth(user, passwd);
    }

    Server::Server(AbstractRouter* r)
        : m_Impl(std::make_unique< ServerImpl >(r))
    {
    }

    Server::~Server()
    {
    }

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
