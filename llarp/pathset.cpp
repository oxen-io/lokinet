#include <dht/messages/pubintro.hpp>
#include <messages/dht.hpp>
#include <path.hpp>
#include <pathset.hpp>

namespace llarp
{
  namespace path
  {
    PathSet::PathSet(size_t num) : m_NumPaths(num)
    {
    }

    bool
    PathSet::ShouldBuildMore(llarp_time_t now) const
    {
      (void)now;
      Lock_t l(m_PathsMutex);
      return m_Paths.size() < m_NumPaths;
    }

    bool
    PathSet::ShouldBuildMoreForRoles(llarp_time_t now, PathRole roles) const
    {
      Lock_t l(m_PathsMutex);
      const size_t required = MinRequiredForRoles(roles);
      size_t has            = 0;
      for(const auto& item : m_Paths)
      {
        if(item.second->SupportsAnyRoles(roles))
        {
          if(!item.second->ExpiresSoon(now))
            ++has;
        }
      }
      return has < required;
    }

    size_t
    PathSet::MinRequiredForRoles(PathRole roles) const
    {
      (void)roles;
      return 0;
    }

    size_t
    PathSet::NumPathsExistingAt(llarp_time_t futureTime) const
    {
      size_t num = 0;
      Lock_t l(m_PathsMutex);
      for(const auto& item : m_Paths)
      {
        if(!item.second->Expired(futureTime))
          ++num;
      }
      return num;
    }

    void
    PathSet::Tick(llarp_time_t now, llarp::Router* r)
    {
      Lock_t l(m_PathsMutex);
      for(auto& item : m_Paths)
      {
        item.second->Tick(now, r);
      }
    }

    void
    PathSet::ExpirePaths(llarp_time_t now)
    {
      Lock_t l(m_PathsMutex);
      if(m_Paths.size() == 0)
        return;
      auto itr = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->Expired(now))
        {
          delete itr->second;
          itr = m_Paths.erase(itr);
        }
        else
          ++itr;
      }
    }

    Path*
    PathSet::GetEstablishedPathClosestTo(const RouterID& id,
                                         PathRole roles) const
    {
      Lock_t l(m_PathsMutex);
      Path* path = nullptr;
      AlignedBuffer< 32 > dist;
      AlignedBuffer< 32 > to = id.data();
      dist.Fill(0xff);
      for(const auto& item : m_Paths)
      {
        if(!item.second->IsReady())
          continue;
        if(!item.second->SupportsAnyRoles(roles))
          continue;
        AlignedBuffer< 32 > localDist = item.second->Endpoint() ^ to;
        if(localDist < dist)
        {
          dist = localDist;
          path = item.second;
        }
      }
      return path;
    }

    Path*
    PathSet::GetNewestPathByRouter(const RouterID& id, PathRole roles) const
    {
      Lock_t l(m_PathsMutex);
      Path* chosen = nullptr;
      auto itr     = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->IsReady() && itr->second->SupportsAnyRoles(roles))
        {
          if(itr->second->Endpoint() == id)
          {
            if(chosen == nullptr)
              chosen = itr->second;
            else if(chosen->intro.expiresAt < itr->second->intro.expiresAt)
              chosen = itr->second;
          }
        }
        ++itr;
      }
      return chosen;
    }

    Path*
    PathSet::GetPathByRouter(const RouterID& id, PathRole roles) const
    {
      Lock_t l(m_PathsMutex);
      Path* chosen = nullptr;
      auto itr     = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->IsReady() && itr->second->SupportsAnyRoles(roles))
        {
          if(itr->second->Endpoint() == id)
          {
            if(chosen == nullptr)
              chosen = itr->second;
            else if(chosen->intro.latency > itr->second->intro.latency)
              chosen = itr->second;
          }
        }
        ++itr;
      }
      return chosen;
    }

    Path*
    PathSet::GetPathByID(const PathID_t& id) const
    {
      Lock_t l(m_PathsMutex);
      auto itr = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->RXID() == id)
          return itr->second;
        ++itr;
      }
      return nullptr;
    }

    size_t
    PathSet::AvailablePaths(PathRole roles) const
    {
      Lock_t l(m_PathsMutex);
      size_t count = 0;
      auto itr     = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->Status() == ePathEstablished
           && itr->second->SupportsAnyRoles(roles))
          ++count;
        ++itr;
      }
      return count;
    }

    size_t
    PathSet::NumInStatus(PathStatus st) const
    {
      Lock_t l(m_PathsMutex);
      size_t count = 0;
      auto itr     = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->Status() == st)
          ++count;
        ++itr;
      }
      return count;
    }

    void
    PathSet::AddPath(Path* path)
    {
      Lock_t l(m_PathsMutex);
      auto upstream = path->Upstream();  // RouterID
      auto RXID     = path->RXID();      // PathID
      m_Paths.insert(std::make_pair(std::make_pair(upstream, RXID), path));
    }

    void
    PathSet::RemovePath(Path* path)
    {
      Lock_t l(m_PathsMutex);
      m_Paths.erase({path->Upstream(), path->RXID()});
    }

    Path*
    PathSet::GetByUpstream(const RouterID& remote, const PathID_t& rxid) const
    {
      Lock_t l(m_PathsMutex);
      auto itr = m_Paths.find({remote, rxid});
      if(itr == m_Paths.end())
        return nullptr;
      return itr->second;
    }

    void
    PathSet::HandlePathBuilt(Path*)
    {
    }

    bool
    PathSet::GetCurrentIntroductionsWithFilter(
        std::set< llarp::service::Introduction >& intros,
        std::function< bool(const llarp::service::Introduction&) > filter) const
    {
      intros.clear();
      size_t count = 0;
      Lock_t l(m_PathsMutex);
      auto itr = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->IsReady() && filter(itr->second->intro))
        {
          intros.insert(itr->second->intro);
          ++count;
        }
        ++itr;
      }
      return count > 0;
    }

    bool
    PathSet::GetCurrentIntroductions(
        std::set< llarp::service::Introduction >& intros) const
    {
      intros.clear();
      size_t count = 0;
      Lock_t l(m_PathsMutex);
      auto itr = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->IsReady())
        {
          intros.insert(itr->second->intro);
          ++count;
        }
        ++itr;
      }
      return count > 0;
    }

    void
    PathSet::HandlePathBuildTimeout(Path* p)
    {
      llarp::LogInfo("path build for ", p->Name(), " has timed out");
    }

    bool
    PathSet::GetNewestIntro(service::Introduction& intro) const
    {
      intro.Clear();
      bool found = false;
      Lock_t l(m_PathsMutex);
      auto itr = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->IsReady()
           && itr->second->intro.expiresAt > intro.expiresAt)
        {
          intro = itr->second->intro;
          found = true;
        }
        ++itr;
      }
      return found;
    }

    Path*
    PathSet::PickRandomEstablishedPath(PathRole roles) const
    {
      std::vector< Path* > established;
      Lock_t l(m_PathsMutex);
      auto itr = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->IsReady() && itr->second->SupportsAnyRoles(roles))
          established.push_back(itr->second);
        ++itr;
      }
      auto sz = established.size();
      if(sz)
      {
        return established[llarp::randint() % sz];
      }
      else
        return nullptr;
    }

  }  // namespace path
}  // namespace llarp
