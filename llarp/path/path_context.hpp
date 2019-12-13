#ifndef LLARP_PATH_CONTEXT_HPP
#define LLARP_PATH_CONTEXT_HPP

#include <crypto/encrypted_frame.hpp>
#include <path/ihophandler.hpp>
#include <path/path_types.hpp>
#include <path/pathset.hpp>
#include <path/transit_hop.hpp>
#include <routing/handler.hpp>
#include <router/i_outbound_message_handler.hpp>
#include <util/compare_ptr.hpp>
#include <util/types.hpp>

#include <memory>
#include <unordered_map>

namespace llarp
{
  class Logic;
  struct AbstractRouter;
  struct LR_CommitMessage;
  struct RelayDownstreamMessage;
  struct RelayUpstreamMessage;
  struct RouterID;

  namespace path
  {
    struct TransitHop;
    struct TransitHopInfo;

    using TransitHop_ptr = std::shared_ptr< TransitHop >;

    struct PathContext
    {
      PathContext(AbstractRouter* router);

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
      AllowingTransit() const;

      bool
      HasTransitHop(const TransitHopInfo& info);

      bool
      HandleRelayCommit(const LR_CommitMessage& msg);

      void
      PutTransitHop(std::shared_ptr< TransitHop > hop);

      HopHandler_ptr
      GetByUpstream(const RouterID& id, const PathID_t& path);

      bool
      TransitHopPreviousIsRouter(const PathID_t& path, const RouterID& r);

      TransitHop_ptr
      GetPathForTransfer(const PathID_t& topath);

      HopHandler_ptr
      GetByDownstream(const RouterID& id, const PathID_t& path);

      PathSet_ptr
      GetLocalPathSet(const PathID_t& id);

      routing::MessageHandler_ptr
      GetHandler(const PathID_t& id);

      using EndpointPathPtrSet = std::set< Path_ptr, ComparePtr< Path_ptr > >;
      /// get a set of all paths that we own who's endpoint is r
      EndpointPathPtrSet
      FindOwnedPathsWithEndpoint(const RouterID& r);

      bool
      ForwardLRCM(const RouterID& nextHop,
                  const std::array< EncryptedFrame, 8 >& frames,
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

      using TransitHopsMap_t =
          std::unordered_multimap< PathID_t, TransitHop_ptr, PathID_t::Hash >;

      struct SyncTransitMap_t
      {
        using Mutex_t = util::NullMutex;
        using Lock_t  = util::NullLock;

        Mutex_t first;  // protects second
        TransitHopsMap_t second GUARDED_BY(first);

        void
        ForEach(std::function< void(const TransitHop_ptr&) > visit)
            LOCKS_EXCLUDED(first)
        {
          Lock_t lock(&first);
          for(const auto& item : second)
            visit(item.second);
        }
      };

      // maps path id -> pathset owner of path
      using OwnedPathsMap_t =
          std::unordered_map< PathID_t, Path_ptr, PathID_t::Hash >;

      struct SyncOwnedPathsMap_t
      {
        using Mutex_t = util::Mutex;
        using Lock_t  = util::Lock;
        Mutex_t first;  // protects second
        OwnedPathsMap_t second GUARDED_BY(first);

        void
        ForEach(std::function< void(const Path_ptr&) > visit)
        {
          Lock_t lock(&first);
          for(const auto& item : second)
            visit(item.second);
        }
      };

      std::shared_ptr< thread::ThreadPool >
      Worker();

      std::shared_ptr< Logic >
      logic();

      AbstractRouter*
      Router();

      const SecretKey&
      EncryptionSecretKey();

      const byte_t*
      OurRouterID() const;

     private:
      AbstractRouter* m_Router;
      SyncTransitMap_t m_TransitPaths;
      SyncOwnedPathsMap_t m_OurPaths;
      bool m_AllowTransit;
    };
  }  // namespace path
}  // namespace llarp

#endif
