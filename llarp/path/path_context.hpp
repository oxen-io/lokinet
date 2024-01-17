#pragma once

#include "abstracthophandler.hpp"
#include "path_types.hpp"
#include "pathset.hpp"
#include "transit_hop.hpp"

#include <llarp/ev/ev.hpp>
#include <llarp/net/ip_address.hpp>
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/decaying_hashset.hpp>
#include <llarp/util/types.hpp>

#include <memory>
#include <unordered_map>

namespace llarp
{
  struct Router;
  struct RouterID;

  namespace path
  {
    struct TransitHop;
    struct TransitHopInfo;

    struct TransitHopID
    {
      RouterID rid;
      PathID_t path_id;

      bool
      operator==(const TransitHopID& other) const
      {
        return rid == other.rid && path_id == other.path_id;
      }
    };
  }  // namespace path
}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::path::TransitHopID>
  {
    size_t
    operator()(const llarp::path::TransitHopID& obj) const noexcept
    {
      return std::hash<llarp::PathID_t>{}(obj.path_id);
    }
  };
}  // namespace std

namespace llarp::path
{
  struct PathContext
  {
    explicit PathContext(Router* router);

    /// called from router tick function
    void
    ExpirePaths(llarp_time_t now);

    void
    allow_transit();

    void
    reject_transit();

    bool
    check_path_limit_hit_by_ip(const IpAddress& ip);

    bool
    is_transit_allowed() const;

    bool
    has_transit_hop(const TransitHopInfo& info);

    void
    put_transit_hop(std::shared_ptr<TransitHop> hop);

    std::shared_ptr<Path>
    get_path(const PathID_t& path_id);

    bool
    TransitHopPreviousIsRouter(const PathID_t& path, const RouterID& r);

    std::shared_ptr<TransitHop>
    GetPathForTransfer(const PathID_t& topath);

    std::shared_ptr<TransitHop>
    GetTransitHop(const RouterID&, const PathID_t&);

    std::shared_ptr<PathSet>
    GetLocalPathSet(const PathID_t& id);

    /// get a set of all paths that we own who's endpoint is r
    std::vector<std::shared_ptr<Path>>
    FindOwnedPathsWithEndpoint(const RouterID& r);

    bool
    HopIsUs(const RouterID& k) const;

    void
    AddOwnPath(std::shared_ptr<PathSet> set, std::shared_ptr<Path> p);

    void
    RemovePathSet(std::shared_ptr<PathSet> set);

    const EventLoop_ptr&
    loop();

    const SecretKey&
    EncryptionSecretKey();

    const byte_t*
    OurRouterID() const;

    /// current number of transit paths we have
    uint64_t
    CurrentTransitPaths();

    /// current number of paths we created in status
    uint64_t
    CurrentOwnedPaths(path::PathStatus status = path::PathStatus::ESTABLISHED);

    Router*
    router() const
    {
      return _router;
    }

   private:
    Router* _router;

    std::unordered_map<TransitHopID, std::shared_ptr<TransitHop>> transit_hops;
    std::unordered_map<PathID_t, std::shared_ptr<Path>> own_paths;
    bool m_AllowTransit;
    util::DecayingHashSet<IpAddress> path_limits;
  };
}  // namespace llarp::path
