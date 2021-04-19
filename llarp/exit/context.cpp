#include "context.hpp"
#include <memory>
#include <stdexcept>

namespace llarp
{
  namespace exit
  {
    Context::Context(AbstractRouter* r) : m_Router(r)
    {}
    Context::~Context() = default;

    void
    Context::Tick(llarp_time_t now)
    {
      {
        auto itr = m_Exits.begin();
        while (itr != m_Exits.end())
        {
          itr->second->Tick(now);
          ++itr;
        }
      }
      {
        auto itr = m_Closed.begin();
        while (itr != m_Closed.end())
        {
          if ((*itr)->ShouldRemove())
            itr = m_Closed.erase(itr);
          else
            ++itr;
        }
      }
    }

    void
    Context::Stop()
    {
      auto itr = m_Exits.begin();
      while (itr != m_Exits.end())
      {
        itr->second->Stop();
        m_Closed.emplace_back(std::move(itr->second));
        itr = m_Exits.erase(itr);
      }
    }

    util::StatusObject
    Context::ExtractStatus() const
    {
      util::StatusObject obj{};
      auto itr = m_Exits.begin();
      while (itr != m_Exits.end())
      {
        obj[itr->first] = itr->second->ExtractStatus();
        ++itr;
      }
      return obj;
    }

    void
    Context::CalculateExitTraffic(TrafficStats& stats)
    {
      auto itr = m_Exits.begin();
      while (itr != m_Exits.end())
      {
        itr->second->CalculateTrafficStats(stats);
        ++itr;
      }
    }

    exit::Endpoint*
    Context::FindEndpointForPath(const PathID_t& path) const
    {
      auto itr = m_Exits.begin();
      while (itr != m_Exits.end())
      {
        auto ep = itr->second->FindEndpointByPath(path);
        if (ep)
          return ep;
        ++itr;
      }
      return nullptr;
    }

    bool
    Context::ObtainNewExit(const PubKey& pk, const PathID_t& path, bool permitInternet)
    {
      auto itr = m_Exits.begin();
      while (itr != m_Exits.end())
      {
        if (itr->second->AllocateNewExit(pk, path, permitInternet))
          return true;
        ++itr;
      }
      return false;
    }

    std::shared_ptr<handlers::ExitEndpoint>
    Context::GetExitEndpoint(std::string name) const
    {
      if (auto itr = m_Exits.find(name); itr != m_Exits.end())
      {
        return itr->second;
      }
      return nullptr;
    }

    void
    Context::AddExitEndpoint(
        const std::string& name, const NetworkConfig& networkConfig, const DnsConfig& dnsConfig)
    {
      if (m_Exits.find(name) != m_Exits.end())
        throw std::invalid_argument(stringify("An exit with name ", name, " already exists"));

      auto endpoint = std::make_unique<handlers::ExitEndpoint>(name, m_Router);
      endpoint->Configure(networkConfig, dnsConfig);

      // add endpoint
      if (!endpoint->Start())
        throw std::runtime_error(stringify("Failed to start endpoint ", name));

      m_Exits.emplace(name, std::move(endpoint));
    }

  }  // namespace exit
}  // namespace llarp
