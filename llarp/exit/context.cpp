#include <exit/context.hpp>
#include <memory>

namespace llarp
{
  namespace exit
  {
    Context::Context(AbstractRouter* r) : m_Router(r)
    {
    }
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

    bool
    Context::AddExitEndpoint(const std::string& name, const Config_t& conf)
    {
      // check for duplicate exit by name
      {
        auto itr = m_Exits.find(name);
        if (itr != m_Exits.end())
        {
          LogError("duplicate exit with name ", name);
          return false;
        }
      }
      std::unique_ptr<handlers::ExitEndpoint> endpoint;
      // make new endpoint
      endpoint = std::make_unique<handlers::ExitEndpoint>(name, m_Router);
      // configure
      {
        auto itr = conf.begin();
        while (itr != conf.end())
        {
          if (!endpoint->SetOption(itr->first, itr->second))
          {
            LogWarn("Couldn't set option ", itr->first, " to  ", itr->second);
            return false;
          }
          ++itr;
        }
      }
      // add endpoint
      if (!endpoint->Start())
      {
        LogWarn("Couldn't start exit endpoint");
        return false;
      }
      m_Exits.emplace(name, std::move(endpoint));
      return true;
    }

  }  // namespace exit
}  // namespace llarp
