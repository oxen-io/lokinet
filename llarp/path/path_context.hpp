#pragma once

#include <llarp/crypto/encrypted_frame.hpp>
#include <llarp/net/ip_address.hpp>
#include "ihophandler.hpp"
#include "path_types.hpp"
#include "pathset.hpp"
#include "transit_hop.hpp"
#include <llarp/routing/handler.hpp>
#include <llarp/router/i_outbound_message_handler.hpp>
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/decaying_hashset.hpp>
#include <llarp/util/types.hpp>

#include <memory>
#include <unordered_map>

namespace llarp
{
  struct AbstractRouter;
  struct LR_CommitMessage;
  struct RelayDownstreamMessage;
  struct RelayUpstreamMessage;
  struct RouterID;

  namespace path
  {
    struct TransitHop;
    struct TransitHopInfo;

    using TransitHop_ptr = std::shared_ptr<TransitHop>;

    struct PathContext
    {
      explicit PathContext(AbstractRouter* router);

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
      AllowingTransit() const;

      bool
      HasTransitHop(const TransitHopInfo& info);

      bool
      HandleRelayCommit(const LR_CommitMessage& msg);

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

      routing::MessageHandler_ptr
      GetHandler(const PathID_t& id);

      using EndpointPathPtrSet = std::set<Path_ptr, ComparePtr<Path_ptr>>;
      /// get a set of all paths that we own who's endpoint is r
      EndpointPathPtrSet
      FindOwnedPathsWithEndpoint(const RouterID& r);

      bool
      ForwardLRCM(
          const RouterID& nextHop,
          const std::array<EncryptedFrame, 8>& frames,
          SendStatusHandler handler);

      bool
      HopIsUs(const RouterID& k) const;

      bool
      HandleLRUM(const RelayUpstreamMessage& msg);

      bool
      HandleLRDM(const RelayDownstreamMessage& msg);

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
        TransitHopsMap_t second GUARDED_BY(first);

        void
        ForEach(std::function<void(const TransitHop_ptr&)> visit) EXCLUDES(first)
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
        OwnedPathsMap_t second GUARDED_BY(first);

        void
        ForEach(std::function<void(const Path_ptr&)> visit)
        {
          util::Lock lock(first);
          for (const auto& item : second)
            visit(item.second);
        }
      };

      const EventLoop_ptr&
      loop();

      AbstractRouter*
      Router();

      const SecretKey&
      EncryptionSecretKey();

      const byte_t*
      OurRouterID() const;

      /// current number of transit paths we have
      uint64_t
      CurrentTransitPaths();

     private:
      AbstractRouter* m_Router;
      SyncTransitMap_t m_TransitPaths;
      SyncOwnedPathsMap_t m_OurPaths;
      bool m_AllowTransit;
      util::DecayingHashSet<IpAddress> m_PathLimits;
    };
  }  // namespace path
}  // namespace llarp
