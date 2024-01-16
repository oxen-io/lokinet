#include "path_context.hpp"

#include "path.hpp"

#include <llarp/router/router.hpp>

namespace llarp::path
{
  static constexpr auto DefaultPathBuildLimit = 500ms;

  PathContext::PathContext(Router* router)
      : _router(router), m_AllowTransit(false), path_limits(DefaultPathBuildLimit)
  {}

  void
  PathContext::allow_transit()
  {
    m_AllowTransit = true;
  }

  bool
  PathContext::is_transit_allowed() const
  {
    return m_AllowTransit;
  }

  bool
  PathContext::check_path_limit_hit_by_ip(const IpAddress& ip)
  {
#ifdef TESTNET
    return false;
#else
    IpAddress remote = ip;
    // null out the port -- we don't care about it for path limiting purposes
    remote.setPort(0);
    // try inserting remote address by ip into decaying hash set
    // if it cannot insert it has hit a limit
    return not path_limits.Insert(remote);
#endif
  }

  const EventLoop_ptr&
  PathContext::loop()
  {
    return _router->loop();
  }

  const SecretKey&
  PathContext::EncryptionSecretKey()
  {
    return _router->encryption();
  }

  bool
  PathContext::HopIsUs(const RouterID& k) const
  {
    return _router->pubkey() == k;
  }

  std::vector<std::shared_ptr<Path>>
  PathContext::FindOwnedPathsWithEndpoint(const RouterID& r)
  {
    std::vector<std::shared_ptr<Path>> found;
    for (const auto& [pathid, path] : own_paths)
    {
      // each path is stored in this map twice, once for each pathid at the first hop
      // This will make the output deduplicated without needing a std::set
      // TODO: we should only need to map one pathid; as the path owner we only send/receive
      //       packets with the first hop's RXID; its TXID is for packets between it and hop 2.
      // TODO: Also, perhaps we want a bit of data duplication here, e.g. a map from
      //       RouterID (terminal hop) to shared_ptr<Path>.
      if (path->TXID() == pathid)
        continue;

      if (path->Endpoint() == r && path->IsReady())
        found.push_back(path);
    }
    return found;
  }

  void
  PathContext::AddOwnPath(PathSet_ptr set, Path_ptr path)
  {
    set->AddPath(path);
    own_paths[path->TXID()] = path;
    own_paths[path->RXID()] = path;
  }

  bool
  PathContext::has_transit_hop(const TransitHopInfo& info)
  {
    TransitHopID downstream{info.downstream, info.rxID};
    if (transit_hops.count(downstream))
      return true;

    TransitHopID upstream{info.upstream, info.txID};
    if (transit_hops.count(upstream))
      return true;

    return false;
  }

  std::shared_ptr<TransitHop>
  PathContext::GetTransitHop(const RouterID& rid, const PathID_t& path_id)
  {
    if (auto itr = transit_hops.find({rid, path_id}); itr != transit_hops.end())
      return itr->second;

    return nullptr;
  }

  Path_ptr
  PathContext::get_path(const PathID_t& path_id)
  {
    if (auto itr = own_paths.find(path_id); itr != own_paths.end())
      return itr->second;

    return nullptr;
  }

  bool
  PathContext::TransitHopPreviousIsRouter(const PathID_t& path_id, const RouterID& otherRouter)
  {
    return transit_hops.count({otherRouter, path_id});
  }

  PathSet_ptr
  PathContext::GetLocalPathSet(const PathID_t& id)
  {
    if (auto itr = own_paths.find(id); itr != own_paths.end())
    {
      if (auto parent = itr->second->m_PathSet.lock())
        return parent;
    }
    return nullptr;
  }

  const byte_t*
  PathContext::OurRouterID() const
  {
    return _router->pubkey();
  }

  std::shared_ptr<TransitHop>
  PathContext::GetPathForTransfer(const PathID_t& id)
  {
    if (auto itr = transit_hops.find({OurRouterID(), id}); itr != transit_hops.end())
    {
      return itr->second;
    }

    return nullptr;
  }

  uint64_t
  PathContext::CurrentTransitPaths()
  {
    return transit_hops.size() / 2;
  }

  uint64_t
  PathContext::CurrentOwnedPaths(path::PathStatus st)
  {
    uint64_t num{};
    for (auto& own_path : own_paths)
    {
      if (own_path.second->Status() == st)
        num++;
    }
    return num / 2;
  }

  void
  PathContext::put_transit_hop(std::shared_ptr<TransitHop> hop)
  {
    TransitHopID downstream{hop->info.downstream, hop->info.rxID};
    TransitHopID upstream{hop->info.upstream, hop->info.txID};
    transit_hops.emplace(std::move(downstream), hop);
    transit_hops.emplace(std::move(upstream), hop);
  }

  void
  PathContext::ExpirePaths(llarp_time_t now)
  {
    // decay limits
    path_limits.Decay(now);

    {
      auto itr = transit_hops.begin();
      while (itr != transit_hops.end())
      {
        if (itr->second->Expired(now))
        {
          // TODO: this
          // _router->outboundMessageHandler().RemovePath(itr->first);
          itr = transit_hops.erase(itr);
        }
        else
        {
          ++itr;
        }
      }
    }
    {
      for (auto itr = own_paths.begin(); itr != own_paths.end();)
      {
        if (itr->second->Expired(now))
        {
          itr = own_paths.erase(itr);
        }
        else
        {
          ++itr;
        }
      }
    }
  }

  void
  PathContext::RemovePathSet(PathSet_ptr)
  {}
}  // namespace llarp::path
