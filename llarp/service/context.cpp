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
    namespace
    {
      using EndpointConstructor = std::function<service::Endpoint_ptr(
          const std::string&, AbstractRouter*, service::Context*)>;
      using EndpointConstructors = std::map<std::string, EndpointConstructor>;

      static EndpointConstructors endpointConstructors = {
          {"tun",
           [](const std::string& nick, AbstractRouter* r, service::Context* c)
               -> service::Endpoint_ptr {
             return std::make_shared<handlers::TunEndpoint>(nick, r, c, false);
           }},
          {"android",
           [](const std::string& nick, AbstractRouter* r, service::Context* c)
               -> service::Endpoint_ptr {
             return std::make_shared<handlers::TunEndpoint>(nick, r, c, true);
           }},
          {"ios",
           [](const std::string& nick, AbstractRouter* r, service::Context* c)
               -> service::Endpoint_ptr {
             return std::make_shared<handlers::TunEndpoint>(nick, r, c, true);
           }},
          {"null",
           [](const std::string& nick, AbstractRouter* r, service::Context* c)
               -> service::Endpoint_ptr {
             return std::make_shared<handlers::NullEndpoint>(nick, r, c);
           }}};

    }  // namespace
    Context::Context(AbstractRouter* r) : m_Router(r)
    {
    }

    Context::~Context() = default;

    bool
    Context::StopAll()
    {
      auto itr = m_Endpoints.begin();
      while (itr != m_Endpoints.end())
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
      while (itr != m_Endpoints.end())
      {
        obj[itr->first] = itr->second->ExtractStatus();
        ++itr;
      }
      return obj;
    }

    void
    Context::ForEachService(
        std::function<bool(const std::string&, const std::shared_ptr<Endpoint>&)> visit) const
    {
      auto itr = m_Endpoints.begin();
      while (itr != m_Endpoints.end())
      {
        if (visit(itr->first, itr->second))
          ++itr;
        else
          return;
      }
    }

    bool
    Context::RemoveEndpoint(const std::string& name)
    {
      auto itr = m_Endpoints.find(name);
      if (itr == m_Endpoints.end())
        return false;
      std::shared_ptr<Endpoint> ep = std::move(itr->second);
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
        while (itr != m_Stopped.end())
        {
          if ((*itr)->ShouldRemove())
            itr = m_Stopped.erase(itr);
          else
            ++itr;
        }
      }
      // tick active endpoints
      for (const auto& item : m_Endpoints)
      {
        item.second->Tick(now);
      }
    }

    bool
    Context::hasEndpoints()
    {
      return m_Endpoints.size() ? true : false;
    }

    static const char*
    DefaultEndpointType()
    {
#ifdef ANDROID
      return "android";
#else
#ifdef IOS
      return "ios";
#else
      return "tun";
#endif
#endif
    }

    bool
    Context::AddDefaultEndpoint(const std::unordered_multimap<std::string, std::string>& opts)
    {
      Config::section_values_t configOpts;
      configOpts.push_back({"type", DefaultEndpointType()});
      // non reachable by default as this is the default endpoint
      // but only if no keyfile option provided
      if (opts.count("keyfile") == 0)
      {
        configOpts.push_back({"reachable", "false"});
      }

      {
        auto itr = opts.begin();
        while (itr != opts.end())
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
      while (itr != m_Endpoints.end())
      {
        if (!itr->second->Start())
        {
          LogError(itr->first, " failed to start");
          return false;
        }
        LogInfo(itr->first, " started");
        ++itr;
      }
      return true;
    }

    Endpoint_ptr
    Context::GetEndpointByName(const std::string& name)
    {
      auto itr = m_Endpoints.find(name);
      if (itr != m_Endpoints.end())
        return itr->second;
      return nullptr;
    }

    bool
    Context::AddEndpoint(const Config::section_t& conf, bool autostart)
    {
      {
        auto itr = m_Endpoints.find(conf.first);
        if (itr != m_Endpoints.end())
        {
          LogError("cannot add hidden service with duplicate name: ", conf.first);
          return false;
        }
      }
      // extract type
      std::string endpointType = DefaultEndpointType();
      std::string keyfile;
      for (const auto& option : conf.second)
      {
        if (option.first == "type")
          endpointType = option.second;
        if (option.first == "keyfile")
          keyfile = option.second;
      }

      service::Endpoint_ptr service;

      {
        // detect type
        const auto itr = endpointConstructors.find(endpointType);
        if (itr == endpointConstructors.end())
        {
          LogError("no such endpoint type: ", endpointType);
          return false;
        }

        // construct
        service = itr->second(conf.first, m_Router, this);
        if (service)
        {
          // if ephemeral, then we need to regen key
          // if privkey file, then set it and load it
          if (!keyfile.empty())
          {
            service->SetOption("keyfile", keyfile);
            // load keyfile, so we have the correct name for logging
          }
          LogInfo("Establishing endpoint identity");
          service->LoadKeyFile();  // only start endpoint not tun
          // now Name() will be correct
        }
      }
      if (service == nullptr)
        return false;
      // configure
      for (const auto& option : conf.second)
      {
        auto& k = option.first;
        if (k == "type")
          continue;
        auto& v = option.second;
        if (!service->SetOption(k, v))
        {
          LogError("failed to set ", k, "=", v, " for hidden service endpoint ", conf.first);
          return false;
        }
      }
      if (autostart)
      {
        // start
        if (service->Start())
        {
          LogInfo("autostarting hidden service endpoint ", service->Name());
          m_Endpoints.emplace(conf.first, service);
          return true;
        }
        LogError("failed to start hidden service endpoint ", conf.first);
        return false;
      }

      LogInfo("added hidden service endpoint ", service->Name());
      m_Endpoints.emplace(conf.first, service);
      return true;
    }
  }  // namespace service
}  // namespace llarp
