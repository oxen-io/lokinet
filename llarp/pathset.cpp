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
      return std::get< 0 >(m_Paths).size() < m_NumPaths;
    }

    void
    PathSet::ExpirePaths(llarp_time_t now)
    {
      {
        auto& map = std::get< 0 >(m_Paths);
        auto itr  = map.begin();
        while(itr != map.end())
        {
          if(itr->second->Expired(now))
          {
            itr = map.erase(itr);
          }
        }
      }
      {
        auto& map = std::get< 1 >(m_Paths);
        auto itr  = map.begin();
        while(itr != map.end())
        {
          if(itr->second->Expired(now))
          {
            // delete path on second iteration
            delete itr->second;
            itr = map.erase(itr);
          }
        }
      }
    }

    size_t
    PathSet::NumInStatus(PathStatus st) const
    {
      size_t count = 0;
      auto& map    = std::get< 0 >(m_Paths);
      auto itr     = map.begin();
      while(itr != map.end())
      {
        if(itr->second->status == st)
          ++count;
        ++itr;
      }
      return count;
    }

    void
    PathSet::AddPath(Path* path)
    {
      std::get< 0 >(m_Paths).emplace(path->TXID(), path);
      std::get< 1 >(m_Paths).emplace(path->RXID(), path);
    }

    void
    PathSet::RemovePath(Path* path)
    {
      std::get< 0 >(m_Paths).erase(path->TXID());
      std::get< 1 >(m_Paths).erase(path->RXID());
    }

    Path*
    PathSet::GetByUpstream(const RouterID& remote, const PathID_t& rxid)
    {
      auto& set = std::get< 1 >(m_Paths);
      auto itr  = set.begin();
      while(itr != set.end())
      {
        if(itr->second->Upstream() == remote)
          return itr->second;
        ++itr;
      }
      return nullptr;
    }

    void
    PathSet::HandlePathBuilt(Path* path)
    {
      // TODO: implement me
    }

  }  // namespace path
}  // namespace llarp