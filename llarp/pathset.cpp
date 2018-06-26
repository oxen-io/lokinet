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
      return m_Tx.size() < m_NumPaths;
    }

    void
    PathSet::ExpirePaths(llarp_time_t now)
    {
      {
        auto itr = m_Rx.begin();
        while(itr != m_Rx.end())
        {
          if(itr->second->Expired(now))
          {
            itr = m_Rx.erase(itr);
          }
          else
            ++itr;
        }
      }
      {
        auto itr = m_Tx.begin();
        while(itr != m_Tx.end())
        {
          if(itr->second->Expired(now))
          {
            // delete path on second iteration
            delete itr->second;
            itr = m_Tx.erase(itr);
          }
          else
            ++itr;
        }
      }
    }

    size_t
    PathSet::NumInStatus(PathStatus st) const
    {
      size_t count = 0;
      auto itr     = m_Tx.begin();
      while(itr != m_Tx.end())
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
      m_Tx.emplace(path->TXID(), path);
      m_Rx.emplace(path->RXID(), path);
    }

    void
    PathSet::RemovePath(Path* path)
    {
      m_Tx.erase(path->TXID());
      m_Rx.erase(path->RXID());
    }

    Path*
    PathSet::GetByUpstream(const RouterID& remote, const PathID_t& rxid)
    {
      auto itr = m_Rx.begin();
      while(itr != m_Rx.end())
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