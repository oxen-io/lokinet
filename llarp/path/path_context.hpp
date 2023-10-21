#pragma once

#include "abstracthophandler.hpp"
#include "path_types.hpp"
#include "pathset.hpp"
#include "transit_hop.hpp"

#include <llarp/crypto/encrypted_frame.hpp>
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
  inline bool
  operator==(const llarp::path::TransitHopID& lhs, const llarp::path::TransitHopID& rhs)
  {
    return lhs.operator==(rhs);
  }

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
  using TransitHop_ptr = std::shared_ptr<TransitHop>;

  struct PathContext
  {
    explicit PathContext(Router* router);

    /// called from router tick function
    void
    ExpirePaths(llarp_time_t now);

    void
    AllowTransit();

    void
    RejectTransit();

    bool
    CheckPathLimitHitByIP(const IpAddress& ip);

    bool
    CheckPathLimitHitByIP(const std::string& ip);

    bool
    AllowingTransit() const;

    bool
    HasTransitHop(const TransitHopInfo& info);

    void
    PutTransitHop(std::shared_ptr<TransitHop> hop);

    Path_ptr
    GetPath(const PathID_t& path_id);

    bool
    TransitHopPreviousIsRouter(const PathID_t& path, const RouterID& r);

    TransitHop_ptr
    GetPathForTransfer(const PathID_t& topath);

    std::shared_ptr<TransitHop>
    GetTransitHop(const RouterID&, const PathID_t&);

    PathSet_ptr
    GetLocalPathSet(const PathID_t& id);

    using EndpointPathPtrSet = std::set<Path_ptr, ComparePtr<Path_ptr>>;
    /// get a set of all paths that we own who's endpoint is r
    EndpointPathPtrSet
    FindOwnedPathsWithEndpoint(const RouterID& r);

    bool
    HopIsUs(const RouterID& k) const;

    void
    AddOwnPath(PathSet_ptr set, Path_ptr p);

    void
    RemovePathSet(PathSet_ptr set);

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
    CurrentOwnedPaths(path::PathStatus status = path::PathStatus::ePathEstablished);

    Router*
    router() const
    {
      return _router;
    }

   private:
    Router* _router;

    std::unordered_map<TransitHopID, TransitHop_ptr> transit_hops;
    std::unordered_map<PathID_t, Path_ptr> own_paths;
    bool m_AllowTransit;
    util::DecayingHashSet<IpAddress> path_limits;
  };
}  // namespace llarp::path
