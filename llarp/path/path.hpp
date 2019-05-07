#ifndef LLARP_PATH_HPP
#define LLARP_PATH_HPP

#include <crypto/encrypted_frame.hpp>
#include <crypto/types.hpp>
#include <messages/relay.hpp>
#include <path/path_types.hpp>
#include <path/pathbuilder.hpp>
#include <path/pathset.hpp>
#include <router_id.hpp>
#include <routing/handler.hpp>
#include <routing/message.hpp>
#include <service/intro.hpp>
#include <util/aligned.hpp>
#include <util/compare_ptr.hpp>
#include <util/threading.hpp>
#include <util/time.hpp>

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

namespace llarp
{
  class Logic;
  struct AbstractRouter;
  struct Crypto;
  struct LR_CommitMessage;
  struct LR_CommitRecord;

  namespace path
  {
    /// maximum path length
    constexpr size_t max_len = 8;
    /// default path length
    constexpr size_t default_len = 4;
    /// pad messages to the nearest this many bytes
    constexpr size_t pad_size = 128;
    /// default path lifetime in ms
    constexpr llarp_time_t default_lifetime = 10 * 60 * 1000;
    /// after this many ms a path build times out
    constexpr llarp_time_t build_timeout = 15000;

    /// measure latency every this interval ms
    constexpr llarp_time_t latency_interval = 5000;

    /// if a path is inactive for this amount of time it's dead
    constexpr llarp_time_t alive_timeout = 60000;

    struct TransitHopInfo
    {
      TransitHopInfo() = default;
      TransitHopInfo(const TransitHopInfo& other);
      TransitHopInfo(const RouterID& down, const LR_CommitRecord& record);

      PathID_t txID, rxID;
      RouterID upstream;
      RouterID downstream;

      std::ostream&
      print(std::ostream& stream, int level, int spaces) const;

      bool
      operator==(const TransitHopInfo& other) const
      {
        return txID == other.txID && rxID == other.rxID
            && upstream == other.upstream && downstream == other.downstream;
      }

      bool
      operator!=(const TransitHopInfo& other) const
      {
        return !(*this == other);
      }

      bool
      operator<(const TransitHopInfo& other) const
      {
        return txID < other.txID || rxID < other.rxID
            || upstream < other.upstream || downstream < other.downstream;
      }

      struct PathIDHash
      {
        std::size_t
        operator()(const PathID_t& a) const
        {
          return AlignedBuffer< PathID_t::SIZE >::Hash()(a);
        }
      };

      struct Hash
      {
        std::size_t
        operator()(TransitHopInfo const& a) const
        {
          std::size_t idx0 = RouterID::Hash()(a.upstream);
          std::size_t idx1 = RouterID::Hash()(a.downstream);
          std::size_t idx2 = PathIDHash()(a.txID);
          std::size_t idx3 = PathIDHash()(a.rxID);
          return idx0 ^ idx1 ^ idx2 ^ idx3;
        }
      };
    };

    inline std::ostream&
    operator<<(std::ostream& out, const TransitHopInfo& info)
    {
      return info.print(out, -1, -1);
    }

    struct IHopHandler
    {
      virtual ~IHopHandler()
      {
      }

      virtual bool
      Expired(llarp_time_t now) const = 0;

      virtual bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const = 0;

      /// send routing message and increment sequence number
      virtual bool
      SendRoutingMessage(const routing::IMessage& msg, AbstractRouter* r) = 0;

      // handle data in upstream direction
      virtual bool
      HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                     AbstractRouter* r) = 0;

      // handle data in downstream direction
      virtual bool
      HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                       AbstractRouter* r) = 0;

      /// return timestamp last remote activity happened at
      virtual llarp_time_t
      LastRemoteActivityAt() const = 0;

      uint64_t
      NextSeqNo()
      {
        return m_SequenceNum++;
      }

     protected:
      uint64_t m_SequenceNum = 0;
    };

    using HopHandler_ptr = std::shared_ptr< IHopHandler >;

    struct TransitHop : public IHopHandler, public routing::IMessageHandler
    {
      TransitHop();

      TransitHop(const TransitHop& other);

      TransitHopInfo info;
      SharedSecret pathKey;
      ShortHash nonceXOR;
      llarp_time_t started = 0;
      // 10 minutes default
      llarp_time_t lifetime = default_lifetime;
      llarp_proto_version_t version;
      llarp_time_t m_LastActivity = 0;

      bool
      IsEndpoint(const RouterID& us) const
      {
        return info.upstream == us;
      }

      llarp_time_t
      ExpireTime() const;

      llarp_time_t
      LastRemoteActivityAt() const override
      {
        return m_LastActivity;
      }

      std::ostream&
      print(std::ostream& stream, int level, int spaces) const;

      bool
      Expired(llarp_time_t now) const override;

      bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const override
      {
        return now >= ExpireTime() - dlt;
      }

      // send routing message when end of path
      bool
      SendRoutingMessage(const routing::IMessage& msg,
                         AbstractRouter* r) override;

      // handle routing message when end of path
      bool
      HandleRoutingMessage(const routing::IMessage& msg, AbstractRouter* r);

      bool
      HandleDataDiscardMessage(const routing::DataDiscardMessage& msg,
                               AbstractRouter* r) override;

      bool
      HandlePathConfirmMessage(const routing::PathConfirmMessage& msg,
                               AbstractRouter* r) override;
      bool
      HandlePathTransferMessage(const routing::PathTransferMessage& msg,
                                AbstractRouter* r) override;
      bool
      HandlePathLatencyMessage(const routing::PathLatencyMessage& msg,
                               AbstractRouter* r) override;

      bool
      HandleObtainExitMessage(const routing::ObtainExitMessage& msg,
                              AbstractRouter* r) override;

      bool
      HandleUpdateExitVerifyMessage(const routing::UpdateExitVerifyMessage& msg,
                                    AbstractRouter* r) override;

      bool
      HandleTransferTrafficMessage(const routing::TransferTrafficMessage& msg,
                                   AbstractRouter* r) override;

      bool
      HandleUpdateExitMessage(const routing::UpdateExitMessage& msg,
                              AbstractRouter* r) override;

      bool
      HandleGrantExitMessage(const routing::GrantExitMessage& msg,
                             AbstractRouter* r) override;
      bool
      HandleRejectExitMessage(const routing::RejectExitMessage& msg,
                              AbstractRouter* r) override;

      bool
      HandleCloseExitMessage(const routing::CloseExitMessage& msg,
                             AbstractRouter* r) override;

      bool
      HandleHiddenServiceFrame(
          ABSL_ATTRIBUTE_UNUSED const service::ProtocolFrame& frame) override
      {
        /// TODO: implement me
        LogWarn("Got hidden service data on transit hop");
        return false;
      }

      bool
      HandleGotIntroMessage(const dht::GotIntroMessage& msg);

      bool
      HandleDHTMessage(const dht::IMessage& msg, AbstractRouter* r) override;

      // handle data in upstream direction
      bool
      HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                     AbstractRouter* r) override;

      // handle data in downstream direction
      bool
      HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                       AbstractRouter* r) override;
    };

    using TransitHop_ptr = std::shared_ptr< TransitHop >;

    inline std::ostream&
    operator<<(std::ostream& out, const TransitHop& h)
    {
      return h.print(out, -1, -1);
    }

    /// configuration for a single hop when building a path
    struct PathHopConfig
    {
      /// path id
      PathID_t txID, rxID;
      // router contact of router
      RouterContact rc;
      // temp public encryption key
      SecretKey commkey;
      /// shared secret at this hop
      SharedSecret shared;
      /// hash of shared secret used for nonce mutation
      ShortHash nonceXOR;
      /// next hop's router id
      RouterID upstream;
      /// nonce for key exchange
      TunnelNonce nonce;
      // lifetime
      llarp_time_t lifetime = default_lifetime;

      ~PathHopConfig();
      PathHopConfig();

      util::StatusObject
      ExtractStatus() const;

      bool
      operator<(const PathHopConfig& other) const
      {
        return txID < other.txID || rxID < other.rxID || rc < other.rc
            || upstream < other.upstream || lifetime < other.lifetime;
      }
    };

    /// A path we made
    struct Path : public IHopHandler,
                  public routing::IMessageHandler,
                  public std::enable_shared_from_this< Path >
    {
      using BuildResultHookFunc = std::function< void(Path_ptr) >;
      using CheckForDeadFunc    = std::function< bool(Path_ptr, llarp_time_t) >;
      using DropHandlerFunc =
          std::function< bool(Path_ptr, const PathID_t&, uint64_t) >;
      using HopList = std::vector< PathHopConfig >;
      using DataHandlerFunc =
          std::function< bool(Path_ptr, const service::ProtocolFrame&) >;
      using ExitUpdatedFunc = std::function< bool(Path_ptr) >;
      using ExitClosedFunc  = std::function< bool(Path_ptr) >;
      using ExitTrafficHandlerFunc =
          std::function< bool(Path_ptr, const llarp_buffer_t&, uint64_t) >;
      /// (path, backoff) backoff is 0 on success
      using ObtainedExitHandler = std::function< bool(Path_ptr, llarp_time_t) >;

      HopList hops;

      PathSet* const m_PathSet;

      service::Introduction intro;

      llarp_time_t buildStarted;

      Path(const std::vector< RouterContact >& routers, PathSet* parent,
           PathRole startingRoles);

      util::StatusObject
      ExtractStatus() const;

      PathRole
      Role() const
      {
        return _role;
      }

      bool
      operator<(const Path& other) const
      {
        const auto sz = hops.size();
        if(sz > other.hops.size())
          return false;
        for(size_t idx = 0; idx < sz; ++idx)
          if(!(hops[idx] < other.hops[idx]))
            return false;
        return true;
      }

      void
      MarkActive(llarp_time_t now)
      {
        m_LastRecvMessage = std::max(now, m_LastRecvMessage);
      }

      /// return true if ALL of the specified roles are supported
      bool
      SupportsAllRoles(PathRole roles) const
      {
        return (_role & roles) == roles;
      }

      /// return true if ANY of the specified roles are supported
      bool
      SupportsAnyRoles(PathRole roles) const
      {
        return roles == ePathRoleAny || (_role & roles) != 0;
      }

      PathStatus
      Status() const
      {
        return _status;
      }

      std::string
      HopsString() const;

      llarp_time_t
      LastRemoteActivityAt() const override
      {
        return m_LastRecvMessage;
      }

      void
      SetBuildResultHook(BuildResultHookFunc func);

      void
      SetExitTrafficHandler(ExitTrafficHandlerFunc handler)
      {
        m_ExitTrafficHandler = handler;
      }

      void
      SetCloseExitFunc(ExitClosedFunc handler)
      {
        m_ExitClosed = handler;
      }

      void
      SetUpdateExitFunc(ExitUpdatedFunc handler)
      {
        m_ExitUpdated = handler;
      }

      void
      SetDataHandler(DataHandlerFunc func)
      {
        m_DataHandler = func;
      }

      void
      SetDropHandler(DropHandlerFunc func)
      {
        m_DropHandler = func;
      }

      void
      SetDeadChecker(CheckForDeadFunc func)
      {
        m_CheckForDead = func;
      }

      void
      EnterState(PathStatus st, llarp_time_t now);

      llarp_time_t
      ExpireTime() const
      {
        return buildStarted + hops[0].lifetime;
      }

      bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt = 5000) const override
      {
        return now >= (ExpireTime() - dlt);
      }

      bool
      Expired(llarp_time_t now) const override;

      /// build a new path on the same set of hops as us
      /// regenerates keys
      void
      Rebuild();

      void
      Tick(llarp_time_t now, AbstractRouter* r);

      bool
      SendRoutingMessage(const routing::IMessage& msg,
                         AbstractRouter* r) override;

      bool
      HandleObtainExitMessage(const routing::ObtainExitMessage& msg,
                              AbstractRouter* r) override;

      bool
      HandleUpdateExitVerifyMessage(const routing::UpdateExitVerifyMessage& msg,
                                    AbstractRouter* r) override;

      bool
      HandleTransferTrafficMessage(const routing::TransferTrafficMessage& msg,
                                   AbstractRouter* r) override;

      bool
      HandleUpdateExitMessage(const routing::UpdateExitMessage& msg,
                              AbstractRouter* r) override;

      bool
      HandleCloseExitMessage(const routing::CloseExitMessage& msg,
                             AbstractRouter* r) override;
      bool
      HandleGrantExitMessage(const routing::GrantExitMessage& msg,
                             AbstractRouter* r) override;
      bool
      HandleRejectExitMessage(const routing::RejectExitMessage& msg,
                              AbstractRouter* r) override;

      bool
      HandleDataDiscardMessage(const routing::DataDiscardMessage& msg,
                               AbstractRouter* r) override;

      bool
      HandlePathConfirmMessage(const routing::PathConfirmMessage& msg,
                               AbstractRouter* r) override;

      bool
      HandlePathLatencyMessage(const routing::PathLatencyMessage& msg,
                               AbstractRouter* r) override;

      bool
      HandlePathTransferMessage(const routing::PathTransferMessage& msg,
                                AbstractRouter* r) override;

      bool
      HandleHiddenServiceFrame(const service::ProtocolFrame& frame) override;

      bool
      HandleGotIntroMessage(const dht::GotIntroMessage& msg);

      bool
      HandleDHTMessage(const dht::IMessage& msg, AbstractRouter* r) override;

      bool
      HandleRoutingMessage(const llarp_buffer_t& buf, AbstractRouter* r);

      // handle data in upstream direction
      bool
      HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                     AbstractRouter* r) override;

      // handle data in downstream direction
      bool
      HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                       AbstractRouter* r) override;

      bool
      IsReady() const;

      // Is this deprecated?
      // nope not deprecated :^DDDD
      PathID_t
      TXID() const;

      RouterID
      Endpoint() const;

      PubKey
      EndpointPubKey() const;

      bool
      IsEndpoint(const RouterID& router, const PathID_t& path) const;

      PathID_t
      RXID() const;

      RouterID
      Upstream() const;

      std::string
      Name() const;

      void
      AddObtainExitHandler(ObtainedExitHandler handler)
      {
        m_ObtainedExitHooks.push_back(handler);
      }

      bool
      SendExitRequest(const routing::ObtainExitMessage& msg, AbstractRouter* r);

      bool
      SendExitClose(const routing::CloseExitMessage& msg, AbstractRouter* r);

     private:
      /// call obtained exit hooks
      bool
      InformExitResult(llarp_time_t b);

      BuildResultHookFunc m_BuiltHook;
      DataHandlerFunc m_DataHandler;
      DropHandlerFunc m_DropHandler;
      CheckForDeadFunc m_CheckForDead;
      ExitUpdatedFunc m_ExitUpdated;
      ExitClosedFunc m_ExitClosed;
      ExitTrafficHandlerFunc m_ExitTrafficHandler;
      std::vector< ObtainedExitHandler > m_ObtainedExitHooks;
      llarp_time_t m_LastRecvMessage     = 0;
      llarp_time_t m_LastLatencyTestTime = 0;
      uint64_t m_LastLatencyTestID       = 0;
      uint64_t m_UpdateExitTX            = 0;
      uint64_t m_CloseExitTX             = 0;
      uint64_t m_ExitObtainTX            = 0;
      PathStatus _status;
      PathRole _role;
    };

    enum PathBuildStatus
    {
      ePathBuildSuccess,
      ePathBuildTimeout,
      ePathBuildReject
    };

    struct PathContext
    {
      PathContext(AbstractRouter* router);
      ~PathContext();

      /// called from router tick function
      void
      ExpirePaths(llarp_time_t now);

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

      HopHandler_ptr
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
                  const std::array< EncryptedFrame, 8 >& frames);

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

      using TransitHopsMap_t = std::multimap< PathID_t, TransitHop_ptr >;

      struct SyncTransitMap_t
      {
        util::Mutex first;  // protects second
        TransitHopsMap_t second GUARDED_BY(first);

        void
        ForEach(std::function< void(const TransitHop_ptr&) > visit)
        {
          util::Lock lock(&first);
          for(const auto& item : second)
            visit(item.second);
        }
      };

      // maps path id -> pathset owner of path
      using OwnedPathsMap_t = std::map< PathID_t, PathSet_ptr >;

      struct SyncOwnedPathsMap_t
      {
        util::Mutex first;  // protects second
        OwnedPathsMap_t second GUARDED_BY(first);

        void
        ForEach(std::function< void(const PathSet_ptr&) > visit)
        {
          util::Lock lock(&first);
          for(const auto& item : second)
            visit(item.second);
        }
      };

      llarp_threadpool*
      Worker();

      llarp::Crypto*
      crypto();

      Logic*
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
