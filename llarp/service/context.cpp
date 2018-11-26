#include <llarp/handlers/tun.hpp>
#include <llarp/service/context.hpp>
#include <llarp/service/endpoint.hpp>
#include "router.hpp"

namespace llarp
{
  namespace service
  {
    Context::Context(llarp_router *r) : m_Router(r)
    {
    }

    Context::~Context()
    {
      auto itr = m_Endpoints.begin();
      while(itr != m_Endpoints.end())
      {
        itr = m_Endpoints.erase(itr);
      }
    }

    void
    Context::Tick(llarp_time_t now)
    {
      auto itr = m_Endpoints.begin();
      while(itr != m_Endpoints.end())
      {
        itr->second->Tick(now);
        ++itr;
      }
    }

    bool
    Context::hasEndpoints()
    {
      return m_Endpoints.size() ? true : false;
    }

    llarp::service::Endpoint *
    Context::getFirstEndpoint()
    {
      if(!m_Endpoints.size())
      {
        llarp::LogError("No endpoints found");
        return nullptr;
      }
      auto itr  = m_Endpoints.begin();
      if(itr == m_Endpoints.end())
        return nullptr;
      return itr->second.get();
    }

    bool
    Context::iterate(struct endpoint_iter &i)
    {
      if(!m_Endpoints.size())
      {
        llarp::LogError("No endpoints found");
        return false;
      }
      i.index = 0;
      // llarp::util::Lock lock(access);
      auto itr = m_Endpoints.begin();
      while(itr != m_Endpoints.end())
      {
        i.endpoint = itr->second.get();
        if(!i.visit(&i))
          return false;
        // advance
        i.index++;
        itr++;
      }
      return true;
    }

    llarp::handlers::TunEndpoint *
    Context::getFirstTun()
    {
      llarp::service::Endpoint *endpointer = this->getFirstEndpoint();
      if(!endpointer)
      {
        return nullptr;
      }
      llarp::handlers::TunEndpoint *tunEndpoint =
          static_cast< llarp::handlers::TunEndpoint * >(endpointer);
      return tunEndpoint;
    }

    llarp_tun_io *
    Context::getRange()
    {
      llarp::handlers::TunEndpoint *tunEndpoint = this->getFirstTun();
      if(!tunEndpoint)
      {
        llarp::LogError("No tunnel endpoint found");
        return nullptr;
      }
      return &tunEndpoint->tunif;
    }

    bool
    Context::FindBestAddressFor(const llarp::service::Address &addr,
                                huint32_t &ip)
    {
      auto itr = m_Endpoints.begin();
      while(itr != m_Endpoints.end())
      {
        if(itr->second->HasAddress(addr.data()))
        {
          ip = itr->second->ObtainIPForAddr(addr.data());
          return true;
        }
        ++itr;
      }
      itr = m_Endpoints.find("default");
      if(itr != m_Endpoints.end())
      {
        ip = itr->second->ObtainIPForAddr(addr.data());
        return true;
      }
      return false;
    }

    bool
    Context::Prefetch(const llarp::service::Address &addr)
    {
      llarp::handlers::TunEndpoint *tunEndpoint = this->getFirstTun();
      if(!tunEndpoint)
      {
        llarp::LogError("No tunnel endpoint found");
        return false;
      }
      // HiddenServiceAddresslookup *lookup = new
      // HiddenServiceEndpoint(tunEndpoint, callback, addr,
      // tunEndpoint->GenTXID());
      return tunEndpoint->EnsurePathToService(
          addr,
          [](__attribute__((unused)) Address addr,
             __attribute__((unused)) void *ctx) {},
          10000);
    }

    huint32_t
    Context::GetIpForAddr(const llarp::service::Address &addr)
    {
      llarp::handlers::TunEndpoint *tunEndpoint = this->getFirstTun();
      if(!tunEndpoint)
      {
        huint32_t zero;
        zero.h = 0;
        llarp::LogError("No tunnel endpoint found");
        return zero;
      }
      return tunEndpoint->ObtainIPForAddr(addr.data());
    }

    bool
    Context::MapAddress(const llarp::service::Address &addr, huint32_t ip)
    {
      if(!m_Endpoints.size())
      {
        llarp::LogError("No endpoints found");
        return false;
      }
      auto firstEndpoint                   = m_Endpoints.begin();
      auto *uniqueEndpoint                 = &firstEndpoint->second;
      llarp::service::Endpoint *endpointer = uniqueEndpoint->get();
      llarp::handlers::TunEndpoint *tunEndpoint =
          static_cast< llarp::handlers::TunEndpoint * >(endpointer);
      if(!tunEndpoint)
      {
        llarp::LogError("No tunnel endpoint found");
        return false;
      }
      return tunEndpoint->MapAddress(addr, ip);
    }

    bool
    MapAddressAllIter(struct Context::endpoint_iter *endpointCfg)
    {
      Context::mapAddressAll_context *context =
          (Context::mapAddressAll_context *)endpointCfg->user;
      llarp::handlers::TunEndpoint *tunEndpoint =
          (llarp::handlers::TunEndpoint *)endpointCfg->endpoint;
      if(!tunEndpoint)
      {
        llarp::LogError("No tunnel endpoint found");
        return true;  // still continue
      }
      return tunEndpoint->MapAddress(context->serviceAddr,
                                     context->localPrivateIpAddr.xtohl());
    }

    bool
    Context::MapAddressAll(const llarp::service::Address &addr,
                           llarp::Addr &localPrivateIpAddr)
    {
      struct Context::mapAddressAll_context context;
      context.serviceAddr        = addr;
      context.localPrivateIpAddr = localPrivateIpAddr;

      struct Context::endpoint_iter i;
      i.user  = &context;
      i.index = 0;
      i.visit = &MapAddressAllIter;
      return this->iterate(i);
    }

    bool
    Context::AddDefaultEndpoint(
        const std::unordered_multimap< std::string, std::string > &opts)
    {
      Config::section_values_t configOpts;
      configOpts.push_back({"type", "tun"});
      {
        auto itr = opts.begin();
        while(itr != opts.end())
        {
          configOpts.push_back({itr->first, itr->second});
          ++itr;
        }
      }
      return AddEndpoint({"default", configOpts});
    }

    bool
    Context::StartAll()
    {
      auto itr = m_Endpoints.begin();
      while(itr != m_Endpoints.end())
      {
        if(!itr->second->Start())
        {
          llarp::LogError(itr->first, " failed to start");
          return false;
        }
        llarp::LogInfo(itr->first, " started");
        ++itr;
      }
      return true;
    }

    bool
    Context::AddEndpoint(const Config::section_t &conf, bool autostart)
    {
      {
        auto itr = m_Endpoints.find(conf.first);
        if(itr != m_Endpoints.end())
        {
          llarp::LogError("cannot add hidden service with duplicate name: ",
                          conf.first);
          return false;
        }
      }
      // extract type
      std::string endpointType = "tun";
      for(const auto &option : conf.second)
      {
        if(option.first == "type")
          endpointType = option.second;
      }
      std::unique_ptr< llarp::service::Endpoint > service;

      static std::map< std::string,
                       std::function< llarp::service::Endpoint *(
                           const std::string &, llarp_router *) > >
          endpointConstructors = {
              {"tun",
               [](const std::string &nick,
                  llarp_router *r) -> llarp::service::Endpoint * {
                 return new llarp::handlers::TunEndpoint(nick, r);
               }},
              {"null",
               [](const std::string &nick,
                  llarp_router *r) -> llarp::service::Endpoint * {
                 return new llarp::service::Endpoint(nick, r);
               }}};

      {
        // detect type
        auto itr = endpointConstructors.find(endpointType);
        if(itr == endpointConstructors.end())
        {
          llarp::LogError("no such endpoint type: ", endpointType);
          return false;
        }

        // construct
        service.reset(itr->second(conf.first, m_Router));
      }
      // configure
      for(const auto &option : conf.second)
      {
        auto &k = option.first;
        if(k == "type")
          continue;
        auto &v = option.second;
        if(!service->SetOption(k, v))
        {
          llarp::LogError("failed to set ", k, "=", v,
                          " for hidden service endpoint ", conf.first);
          return false;
        }
      }
      if(autostart)
      {
        // start
        if(service->Start())
        {
          llarp::LogInfo("autostarting hidden service endpoint ",
                         service->Name());
          m_Endpoints.insert(std::make_pair(conf.first, std::move(service)));
          return true;
        }
        llarp::LogError("failed to start hidden service endpoint ", conf.first);
        return false;
      }
      else
      {
        llarp::LogInfo("added hidden service endpoint ", service->Name());
        m_Endpoints.insert(std::make_pair(conf.first, std::move(service)));
        return true;
      }
    }
  }  // namespace service
}  // namespace llarp
