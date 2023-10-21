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
  PathContext::AllowTransit()
  {
    m_AllowTransit = true;
  }

  bool
  PathContext::AllowingTransit() const
  {
    return m_AllowTransit;
  }

  bool
  PathContext::CheckPathLimitHitByIP(const IpAddress& ip)
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

  bool
  PathContext::CheckPathLimitHitByIP(const std::string& ip)
  {
#ifdef TESTNET
    return false;
#else
    IpAddress remote{ip};
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

  PathContext::EndpointPathPtrSet
  PathContext::FindOwnedPathsWithEndpoint(const RouterID& r)
  {
    EndpointPathPtrSet found;
    for (const auto& [pathid, path] : own_paths)
    {
      if (path->Endpoint() == r && path->IsReady())
        found.insert(path);
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
  PathContext::HasTransitHop(const TransitHopInfo& info)
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
  PathContext::GetPath(const PathID_t& path_id)
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

  TransitHop_ptr
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
  PathContext::PutTransitHop(std::shared_ptr<TransitHop> hop)
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
          itr->second->DecayFilters(now);
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
          itr->second->DecayFilters(now);
          ++itr;
        }
      }
    }
  }

  void
  PathContext::RemovePathSet(PathSet_ptr)
  {}
}  // namespace llarp::path
