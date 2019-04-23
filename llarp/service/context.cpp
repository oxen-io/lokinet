#include <service/context.hpp>

#include <handlers/null.hpp>
#include <handlers/tun.hpp>
#include <nodedb.hpp>
#include <router/abstractrouter.hpp>
#include <service/endpoint.hpp>

namespace llarp
{
  namespace service
  {
    Context::Context(AbstractRouter *r) : m_Router(r)
    {
    }

    Context::~Context()
    {
    }

    bool
    Context::StopAll()
    {
      auto itr = m_Endpoints.begin();
      while(itr != m_Endpoints.end())
      {
        itr->second->Stop();
        m_Stopped.emplace_back(std::move(itr->second));
        itr = m_Endpoints.erase(itr);
      }
      return true;
    }

    util::StatusObject
    Context::ExtractStatus() const
    {
      util::StatusObject obj{};
      auto itr = m_Endpoints.begin();
      while(itr != m_Endpoints.end())
      {
        obj.Put(itr->first, itr->second->ExtractStatus());
        ++itr;
      }
      return obj;
    }

    void
    Context::ForEachService(
        std::function< bool(const std::string &,
                            const std::unique_ptr< Endpoint > &) >
            visit) const
    {
      auto itr = m_Endpoints.begin();
      while(itr != m_Endpoints.end())
      {
        if(visit(itr->first, itr->second))
          ++itr;
        else
          return;
      }
    }

    bool
    Context::RemoveEndpoint(const std::string &name)
    {
      auto itr = m_Endpoints.find(name);
      if(itr == m_Endpoints.end())
        return false;
      std::unique_ptr< Endpoint > ep = std::move(itr->second);
      m_Endpoints.erase(itr);
      ep->Stop();
      m_Stopped.emplace_back(std::move(ep));
      return true;
    }

    void
    Context::Tick(llarp_time_t now)
    {
      // erase stopped endpoints that are done
      {
        auto itr = m_Stopped.begin();
        while(itr != m_Stopped.end())
        {
          if((*itr)->ShouldRemove())
            itr = m_Stopped.erase(itr);
          else
            ++itr;
        }
      }
      // tick active endpoints
      {
        auto itr = m_Endpoints.begin();
        while(itr != m_Endpoints.end())
        {
          itr->second->Tick(now);
          ++itr;
        }
      }

      auto ep = getFirstEndpoint();
      if(!ep)
        return;
      std::vector< RouterID > expired;
      m_Router->nodedb()->visit([&](const RouterContact &rc) -> bool {
        if(rc.IsExpired(now))
          expired.emplace_back(rc.pubkey);
        return true;
      });
      // TODO: we need to stop looking up service nodes that are gone forever
      // how do?
      for(const auto &k : expired)
        ep->LookupRouterAnon(k);
    }

    bool
    Context::hasEndpoints()
    {
      return m_Endpoints.size() ? true : false;
    }

    service::Endpoint *
    Context::getFirstEndpoint()
    {
      if(!m_Endpoints.size())
      {
        LogError("No endpoints found");
        return nullptr;
      }
      auto itr = m_Endpoints.begin();
      if(itr == m_Endpoints.end())
        return nullptr;
      return itr->second.get();
    }

    bool
    Context::iterate(struct endpoint_iter &i)
    {
      if(!m_Endpoints.size())
      {
        LogError("No endpoints found");
        return false;
      }
      i.index = 0;
      // util::Lock lock(access);
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

    handlers::TunEndpoint *
    Context::getFirstTun()
    {
      service::Endpoint *endpointer = this->getFirstEndpoint();
      if(!endpointer)
      {
        return nullptr;
      }
      handlers::TunEndpoint *tunEndpoint =
          static_cast< handlers::TunEndpoint * >(endpointer);
      return tunEndpoint;
    }

    llarp_tun_io *
    Context::getRange()
    {
      handlers::TunEndpoint *tunEndpoint = this->getFirstTun();
      if(!tunEndpoint)
      {
        LogError("No tunnel endpoint found");
        return nullptr;
      }
      return &tunEndpoint->tunif;
    }

    bool
    Context::FindBestAddressFor(const AlignedBuffer< 32 > &addr, bool isSNode,
                                huint32_t &ip)
    {
      auto itr = m_Endpoints.begin();
      while(itr != m_Endpoints.end())
      {
        if(itr->second->HasAddress(addr))
        {
          ip = itr->second->ObtainIPForAddr(addr, isSNode);
          return true;
        }
        ++itr;
      }
      itr = m_Endpoints.find("default");
      if(itr != m_Endpoints.end())
      {
        ip = itr->second->ObtainIPForAddr(addr, isSNode);
        return true;
      }
      return false;
    }

    bool
    Context::Prefetch(const service::Address &addr)
    {
      handlers::TunEndpoint *tunEndpoint = this->getFirstTun();
      if(!tunEndpoint)
      {
        LogError("No tunnel endpoint found");
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

    bool
    MapAddressAllIter(struct Context::endpoint_iter *endpointCfg)
    {
      Context::mapAddressAll_context *context =
          (Context::mapAddressAll_context *)endpointCfg->user;
      handlers::TunEndpoint *tunEndpoint =
          (handlers::TunEndpoint *)endpointCfg->endpoint;
      if(!tunEndpoint)
      {
        LogError("No tunnel endpoint found");
        return true;  // still continue
      }
      return tunEndpoint->MapAddress(
          context->serviceAddr, context->localPrivateIpAddr.xtohl(), false);
    }

    bool
    Context::MapAddressAll(const service::Address &addr,
                           Addr &localPrivateIpAddr)
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
          LogError(itr->first, " failed to start");
          return false;
        }
        LogInfo(itr->first, " started");
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
          LogError("cannot add hidden service with duplicate name: ",
                   conf.first);
          return false;
        }
      }
      // extract type
      std::string endpointType = "tun";
      std::string keyfile      = "";
      for(const auto &option : conf.second)
      {
        if(option.first == "type")
          endpointType = option.second;
        if(option.first == "keyfile")
          keyfile = option.second;
      }

      std::unique_ptr< service::Endpoint > service;

      static std::map<
          std::string,
          std::function< std::unique_ptr< service::Endpoint >(
              const std::string &, AbstractRouter *, service::Context *) > >
          endpointConstructors = {
              {"tun",
               [](const std::string &nick, AbstractRouter *r,
                  service::Context *c) -> std::unique_ptr< service::Endpoint > {
                 return std::make_unique< handlers::TunEndpoint >(nick, r, c);
               }},
              {"null",
               [](const std::string &nick, AbstractRouter *r,
                  service::Context *c) -> std::unique_ptr< service::Endpoint > {
                 return std::make_unique< handlers::NullEndpoint >(nick, r, c);
               }}};

      {
        // detect type
        auto itr = endpointConstructors.find(endpointType);
        if(itr == endpointConstructors.end())
        {
          LogError("no such endpoint type: ", endpointType);
          return false;
        }

        // construct
        service = itr->second(conf.first, m_Router, this);

        // if ephemeral, then we need to regen key
        // if privkey file, then set it and load it
        if(keyfile != "")
        {
          service->SetOption("keyfile", keyfile);
          // load keyfile, so we have the correct name for logging
        }
        LogInfo("Establishing endpoint identity");
        service->LoadKeyFile();  // only start endpoint not tun
        // now Name() will be correct
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
          LogError("failed to set ", k, "=", v, " for hidden service endpoint ",
                   conf.first);
          return false;
        }
      }
      if(autostart)
      {
        // start
        if(service->Start())
        {
          LogInfo("autostarting hidden service endpoint ", service->Name());
          m_Endpoints.emplace(conf.first, std::move(service));
          return true;
        }
        LogError("failed to start hidden service endpoint ", conf.first);
        return false;
      }
      else
      {
        LogInfo("added hidden service endpoint ", service->Name());
        m_Endpoints.emplace(conf.first, std::move(service));
        return true;
      }
    }
  }  // namespace service
}  // namespace llarp
