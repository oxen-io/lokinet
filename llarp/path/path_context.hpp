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

    using TransitHop_ptr = std::shared_ptr<TransitHop>;

    struct PathContext
    {
      explicit PathContext(Router* router);

      /// called from router tick function
      void
      ExpirePaths(llarp_time_t now);

      void
      PumpUpstream();

      void
      PumpDownstream();

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

      HopHandler_ptr
      GetByUpstream(const RouterID& id, const PathID_t& path);

      bool
      TransitHopPreviousIsRouter(const PathID_t& path, const RouterID& r);

      TransitHop_ptr
      GetPathForTransfer(const PathID_t& topath);

      HopHandler_ptr
      GetByDownstream(const RouterID& id, const PathID_t& path);

      std::optional<std::weak_ptr<TransitHop>>
      TransitHopByInfo(const TransitHopInfo&);

      std::optional<std::weak_ptr<TransitHop>>
      TransitHopByUpstream(const RouterID&, const PathID_t&);

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

      using TransitHopsMap_t = std::unordered_multimap<PathID_t, TransitHop_ptr>;

      struct SyncTransitMap_t
      {
        using Mutex_t = util::NullMutex;
        using Lock_t = util::NullLock;

        Mutex_t first;  // protects second
        TransitHopsMap_t second;

        /// Invokes a callback for each transit path; visit must be invokable with a `const
        /// TransitHop_ptr&` argument.
        template <typename TransitHopVisitor>
        void
        ForEach(TransitHopVisitor&& visit)
        {
          Lock_t lock(first);
          for (const auto& item : second)
            visit(item.second);
        }
      };

      // maps path id -> pathset owner of path
      using OwnedPathsMap_t = std::unordered_map<PathID_t, Path_ptr>;

      struct SyncOwnedPathsMap_t
      {
        util::Mutex first;  // protects second
        OwnedPathsMap_t second;

        /// Invokes a callback for each owned path; visit must be invokable with a `const Path_ptr&`
        /// argument.
        template <typename OwnedHopVisitor>
        void
        ForEach(OwnedHopVisitor&& visit)
        {
          util::Lock lock(first);
          for (const auto& item : second)
            visit(item.second);
        }
      };

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
      SyncTransitMap_t m_TransitPaths;
      SyncOwnedPathsMap_t m_OurPaths;
      bool m_AllowTransit;
      util::DecayingHashSet<IpAddress> path_limits;
    };
  }  // namespace path
}  // namespace llarp
