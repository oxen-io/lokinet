#include <llarp/dht/messages/pubintro.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/path.hpp>
#include <llarp/pathset.hpp>

namespace llarp
{
  namespace path
  {
    PathSet::PathSet(size_t num) : m_NumPaths(num)
    {
    }

    bool
    PathSet::ShouldBuildMore() const
    {
      return m_Paths.size() < m_NumPaths;
    }

    void
    PathSet::Tick(llarp_time_t now, llarp_router* r)
    {
      for(auto& item : m_Paths)
      {
        item.second->Tick(now, r);
      }
    }

    void
    PathSet::ExpirePaths(llarp_time_t now)
    {
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
    PathSet::GetEstablishedPathClosestTo(const AlignedBuffer< 32 >& id) const
    {
      Path* path = nullptr;
      AlignedBuffer< 32 > dist;
      dist.Fill(0xff);
      for(const auto& item : m_Paths)
      {
        if(!item.second->IsReady())
          continue;
        AlignedBuffer< 32 > localDist = item.second->Endpoint() ^ id;
        if(localDist < dist)
        {
          dist = localDist;
          path = item.second;
        }
      }
      return path;
    }

    Path*
    PathSet::GetNewestPathByRouter(const RouterID& id) const
    {
      Path* chosen = nullptr;
      auto itr     = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->IsReady())
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
    PathSet::GetPathByRouter(const RouterID& id) const
    {
      Path* chosen = nullptr;
      auto itr     = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->IsReady())
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
    PathSet::NumInStatus(PathStatus st) const
    {
      size_t count = 0;
      auto itr     = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->_status == st)
          ++count;
        ++itr;
      }
      return count;
    }

    void
    PathSet::AddPath(Path* path)
    {
      m_Paths.insert(
          std::make_pair(std::make_pair(path->Upstream(), path->RXID()), path));
    }

    void
    PathSet::RemovePath(Path* path)
    {
      m_Paths.erase({path->Upstream(), path->RXID()});
    }

    Path*
    PathSet::GetByUpstream(const RouterID& remote, const PathID_t& rxid) const
    {
      auto itr = m_Paths.find({remote, rxid});
      if(itr == m_Paths.end())
        return nullptr;
      return itr->second;
    }

    void
    PathSet::HandlePathBuilt(Path* path)
    {
      auto dlt = llarp_time_now_ms() - path->buildStarted;
      llarp::LogInfo("Path build took ", dlt, "ms for tx=", path->TXID(),
                     " rx=", path->RXID());
    }

    bool
    PathSet::GetCurrentIntroductions(
        std::set< llarp::service::Introduction >& intros) const
    {
      intros.clear();
      size_t count = 0;
      auto itr     = m_Paths.begin();
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
      llarp::LogInfo("path ", p->Name(), " has timed out");
    }

    bool
    PathSet::GetNewestIntro(service::Introduction& intro) const
    {
      intro.Clear();
      bool found = false;
      auto itr   = m_Paths.begin();
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
    PathSet::PickRandomEstablishedPath() const
    {
      std::vector< Path* > established;
      auto itr = m_Paths.begin();
      while(itr != m_Paths.end())
      {
        if(itr->second->IsReady())
          established.push_back(itr->second);
        ++itr;
      }
      auto sz = established.size();
      if(sz)
      {
        return established[llarp_randint() % sz];
      }
      else
        return nullptr;
    }

  }  // namespace path
}  // namespace llarp
