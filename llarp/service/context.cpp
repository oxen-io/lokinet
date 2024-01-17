#include "context.hpp"

#include "endpoint.hpp"

#include <llarp/handlers/null.hpp>
#include <llarp/handlers/tun.hpp>

#include <stdexcept>

namespace llarp::service
{
  static auto logcat = log::Cat("service");
  namespace
  {
    using EndpointConstructor =
        std::function<std::shared_ptr<Endpoint>(Router*, service::Context*)>;
    using EndpointConstructors = std::map<std::string, EndpointConstructor>;

    static EndpointConstructors endpointConstructors = {
        {"tun",
         [](Router* r, service::Context* c) {
           return std::make_shared<handlers::TunEndpoint>(r, c);
         }},
        {"android",
         [](Router* r, service::Context* c) {
           return std::make_shared<handlers::TunEndpoint>(r, c);
         }},
        {"ios",
         [](Router* r, service::Context* c) {
           return std::make_shared<handlers::TunEndpoint>(r, c);
         }},
        {"null", [](Router* r, service::Context* c) {
           return std::make_shared<handlers::NullEndpoint>(r, c);
         }}};

  }  // namespace
  Context::Context(Router* r) : m_Router(r)
  {}

  Context::~Context() = default;

  bool
  Context::StopAll()
  {
    auto itr = m_Endpoints.begin();
    while (itr != m_Endpoints.end())
    {
      log::debug(logcat, "Stopping endpoint {}.", itr->first);
      itr->second->Stop();
      log::debug(logcat, "Endpoint {} stopped.", itr->first);
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

  void
  Context::Pump()
  {
    auto now = time_now_ms();
    for (auto& [name, endpoint] : m_Endpoints)
      endpoint->Pump(now);
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

  std::shared_ptr<Endpoint>
  Context::GetEndpointByName(const std::string& name) const
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
  Context::AddEndpoint(const Config& conf, bool autostart)
  {
    constexpr auto endpointName = "default";

    if (m_Endpoints.find(endpointName) != m_Endpoints.end())
      throw std::invalid_argument("service::Context only supports one endpoint now");

    const auto& endpointType = conf.network.endpoint_type;
    // use factory to create endpoint
    const auto itr = endpointConstructors.find(endpointType);
    if (itr == endpointConstructors.end())
      throw std::invalid_argument{fmt::format("Endpoint type {} does not exist", endpointType)};

    auto service = itr->second(m_Router, this);
    if (not service)
      throw std::runtime_error{
          fmt::format("Failed to construct endpoint of type {}", endpointType)};

    // pass conf to service
    service->Configure(conf.network, conf.dns);

    if (not service->LoadKeyFile())
      throw std::runtime_error("Endpoint's keyfile could not be loaded");

    // autostart if requested
    if (autostart)
    {
      if (service->Start())
        LogInfo("autostarting hidden service endpoint ", service->Name());
      else
        throw std::runtime_error("failed to start hidden service endpoint");
    }

    m_Endpoints.emplace(endpointName, service);
  }
}  // namespace llarp::service
