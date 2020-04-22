#include <service/context.hpp>

#include <handlers/null.hpp>
#include <handlers/tun.hpp>
#include <nodedb.hpp>
#include <router/abstractrouter.hpp>
#include <service/endpoint.hpp>
#include <stdexcept>

namespace llarp
{
  namespace service
  {
    namespace
    {
      using EndpointConstructor = std::function<service::Endpoint_ptr(
          const SnappConfig&, AbstractRouter*, service::Context*)>;
      using EndpointConstructors = std::map<std::string, EndpointConstructor>;

      static EndpointConstructors endpointConstructors = {
          {"tun",
           [](const SnappConfig& conf, AbstractRouter* r, service::Context* c) {
             return std::make_shared<handlers::TunEndpoint>(conf, r, c, false);
           }},
          {"android",
           [](const SnappConfig& conf, AbstractRouter* r, service::Context* c) {
             return std::make_shared<handlers::TunEndpoint>(conf, r, c, true);
           }},
          {"ios",
           [](const SnappConfig& conf, AbstractRouter* r, service::Context* c) {
             return std::make_shared<handlers::TunEndpoint>(conf, r, c, true);
           }},
          {"null", [](const SnappConfig& conf, AbstractRouter* r, service::Context* c) {
             return std::make_shared<handlers::NullEndpoint>(conf, r, c);
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
    Context::AddDefaultEndpoint(const std::unordered_multimap<std::string, std::string>&)
    {
      throw std::runtime_error("FIXME");
      /*
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
      */
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

    void
    Context::InjectEndpoint(std::string name, std::shared_ptr<Endpoint> ep)
    {
      ep->LoadKeyFile();
      if (ep->Start())
      {
        m_Endpoints.emplace(std::move(name), std::move(ep));
      }
    }

    void
    Context::AddEndpoint(const SnappConfig& conf, bool autostart)
    {
      if (m_Endpoints.find(conf.m_name) != m_Endpoints.end())
        throw std::invalid_argument(stringify("Snapp ", conf.m_name, " already exists"));

      // use specified type, fall back to default
      std::string endpointType = conf.m_name;
      if (endpointType.empty())
        endpointType = DefaultEndpointType();

      // use factory to create endpoint
      const auto itr = endpointConstructors.find(endpointType);
      if (itr == endpointConstructors.end())
        throw std::invalid_argument(stringify("Endpoint type ", endpointType, " does not exist"));

      auto service = itr->second(conf, m_Router, this);
      if (not service)
        throw std::runtime_error(stringify(
            "Failed to create endpoint service for ",
            conf.m_name,
            "(type: ",
            conf.m_endpointType,
            ")"));

      // pass conf to service
      service->Configure(conf);

      // autostart if requested
      if (autostart)
      {
        // start
        if (service->Start())
        {
          LogInfo("autostarting hidden service endpoint ", service->Name());
          m_Endpoints.emplace(conf.m_name, service);
        }
        else
        {
          throw std::runtime_error(
              stringify("failed to start hidden service endpoint ", conf.m_name));
        }
      }

      LogInfo("added hidden service endpoint ", service->Name());
      m_Endpoints.emplace(conf.m_name, service);
    }
  }  // namespace service
}  // namespace llarp
