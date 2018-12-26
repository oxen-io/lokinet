#ifndef LLARP_PATH_HPP
#define LLARP_PATH_HPP

#include <aligned.hpp>
#include <crypto.hpp>
#include <dht.hpp>
#include <messages/relay.hpp>
#include <messages/relay_commit.hpp>
#include <path_types.hpp>
#include <pathbuilder.hpp>
#include <pathset.hpp>
#include <router_id.hpp>
#include <routing/handler.hpp>
#include <routing/message.hpp>
#include <service/Intro.hpp>
#include <threading.hpp>
#include <time.hpp>

#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

#define MAXHOPS (8)
#define DEFAULT_PATH_LIFETIME (10 * 60 * 1000)
#define PATH_BUILD_TIMEOUT (30 * 1000)
#define MESSAGE_PAD_SIZE (128)
#define PATH_ALIVE_TIMEOUT (10 * 1000)

namespace llarp
{
  namespace path
  {
    struct TransitHopInfo
    {
      TransitHopInfo() = default;
      TransitHopInfo(const TransitHopInfo& other);
      TransitHopInfo(const RouterID& down, const LR_CommitRecord& record);

      PathID_t txID, rxID;
      RouterID upstream;
      RouterID downstream;

      friend std::ostream&
      operator<<(std::ostream& out, const TransitHopInfo& info)
      {
        out << "<tx=" << info.txID << " rx=" << info.rxID;
        out << " upstream=" << info.upstream
            << " downstream=" << info.downstream;
        return out << ">";
      }

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

      struct Hash
      {
        std::size_t
        operator()(TransitHopInfo const& a) const
        {
          std::size_t idx0, idx1, idx2, idx3;
          memcpy(&idx0, a.upstream, sizeof(std::size_t));
          memcpy(&idx1, a.downstream, sizeof(std::size_t));
          memcpy(&idx2, a.txID, sizeof(std::size_t));
          memcpy(&idx3, a.rxID, sizeof(std::size_t));
          return idx0 ^ idx1 ^ idx2;
        }
      };
    };

    struct PathIDHash
    {
      std::size_t
      operator()(const PathID_t& a) const
      {
        std::size_t idx0;
        memcpy(&idx0, a, sizeof(std::size_t));
        return idx0;
      }
    };

    struct IHopHandler
    {
      virtual ~IHopHandler(){};

      virtual bool
      Expired(llarp_time_t now) const = 0;

      virtual bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const = 0;

      /// send routing message and increment sequence number
      virtual bool
      SendRoutingMessage(const llarp::routing::IMessage* msg,
                         llarp::Router* r) = 0;

      // handle data in upstream direction
      virtual bool
      HandleUpstream(llarp_buffer_t X, const TunnelNonce& Y,
                     llarp::Router* r) = 0;

      // handle data in downstream direction
      virtual bool
      HandleDownstream(llarp_buffer_t X, const TunnelNonce& Y,
                       llarp::Router* r) = 0;

      uint64_t
      NextSeqNo()
      {
        return m_SequenceNum++;
      }

     protected:
      uint64_t m_SequenceNum = 0;
    };

    struct TransitHop : public IHopHandler,
                        public llarp::routing::IMessageHandler
    {
      TransitHop();

      TransitHop(const TransitHop& other);

      TransitHopInfo info;
      SharedSecret pathKey;
      ShortHash nonceXOR;
      llarp_time_t started = 0;
      // 10 minutes default
      llarp_time_t lifetime = DEFAULT_PATH_LIFETIME;
      llarp_proto_version_t version;

      bool
      IsEndpoint(const RouterID& us) const
      {
        return info.upstream == us;
      }

      llarp_time_t
      ExpireTime() const;
      llarp::routing::InboundMessageParser m_MessageParser;

      friend std::ostream&
      operator<<(std::ostream& out, const TransitHop& h)
      {
        return out << "[TransitHop " << h.info << " started=" << h.started
                   << " lifetime=" << h.lifetime << "]";
      }

      bool
      Expired(llarp_time_t now) const override;

      bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const override
      {
        return now >= ExpireTime() - dlt;
      }

      // send routing message when end of path
      bool
      SendRoutingMessage(const llarp::routing::IMessage* msg,
                         llarp::Router* r) override;

      // handle routing message when end of path
      bool
      HandleRoutingMessage(const llarp::routing::IMessage* msg,
                           llarp::Router* r);

      bool
      HandleDataDiscardMessage(const llarp::routing::DataDiscardMessage* msg,
                               llarp::Router* r) override;

      bool
      HandlePathConfirmMessage(const llarp::routing::PathConfirmMessage* msg,
                               llarp::Router* r) override;
      bool
      HandlePathTransferMessage(const llarp::routing::PathTransferMessage* msg,
                                llarp::Router* r) override;
      bool
      HandlePathLatencyMessage(const llarp::routing::PathLatencyMessage* msg,
                               llarp::Router* r) override;

      bool
      HandleObtainExitMessage(const llarp::routing::ObtainExitMessage* msg,
                              llarp::Router* r) override;

      bool
      HandleUpdateExitVerifyMessage(
          const llarp::routing::UpdateExitVerifyMessage* msg,
          llarp::Router* r) override;

      bool
      HandleTransferTrafficMessage(
          const llarp::routing::TransferTrafficMessage* msg,
          llarp::Router* r) override;

      bool
      HandleUpdateExitMessage(const llarp::routing::UpdateExitMessage* msg,
                              llarp::Router* r) override;

      bool
      HandleGrantExitMessage(const llarp::routing::GrantExitMessage* msg,
                             llarp::Router* r) override;
      bool
      HandleRejectExitMessage(const llarp::routing::RejectExitMessage* msg,
                              llarp::Router* r) override;

      bool
      HandleCloseExitMessage(const llarp::routing::CloseExitMessage* msg,
                             llarp::Router* r) override;

      bool
      HandleHiddenServiceFrame(__attribute__((
          unused)) const llarp::service::ProtocolFrame* frame) override
      {
        /// TODO: implement me
        llarp::LogWarn("Got hidden service data on transit hop");
        return false;
      }

      bool
      HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg);

      bool
      HandleDHTMessage(const llarp::dht::IMessage* msg,
                       llarp::Router* r) override;

      // handle data in upstream direction
      bool
      HandleUpstream(llarp_buffer_t X, const TunnelNonce& Y,
                     llarp::Router* r) override;

      // handle data in downstream direction
      bool
      HandleDownstream(llarp_buffer_t X, const TunnelNonce& Y,
                       llarp::Router* r) override;
    };

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
      llarp_time_t lifetime = DEFAULT_PATH_LIFETIME;

      ~PathHopConfig();
      PathHopConfig();
    };

    /// A path we made
    struct Path : public IHopHandler, public llarp::routing::IMessageHandler
    {
      using BuildResultHookFunc = std::function< void(Path*) >;
      using CheckForDeadFunc    = std::function< bool(Path*, llarp_time_t) >;
      using DropHandlerFunc =
          std::function< bool(Path*, const PathID_t&, uint64_t) >;
      using HopList = std::vector< PathHopConfig >;
      using DataHandlerFunc =
          std::function< bool(Path*, const service::ProtocolFrame*) >;
      using ExitUpdatedFunc = std::function< bool(Path*) >;
      using ExitClosedFunc  = std::function< bool(Path*) >;
      using ExitTrafficHandlerFunc =
          std::function< bool(Path*, llarp_buffer_t, uint64_t) >;
      /// (path, backoff) backoff is 0 on success
      using ObtainedExitHandler = std::function< bool(Path*, llarp_time_t) >;

      HopList hops;

      PathSet* m_PathSet;

      llarp::service::Introduction intro;

      llarp_time_t buildStarted;

      Path(const std::vector< RouterContact >& routers, PathSet* parent,
           PathRole startingRoles);

      PathRole
      Role() const
      {
        return _role;
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

      void
      Tick(llarp_time_t now, llarp::Router* r);

      bool
      SendRoutingMessage(const llarp::routing::IMessage* msg,
                         llarp::Router* r) override;

      bool
      HandleObtainExitMessage(const llarp::routing::ObtainExitMessage* msg,
                              llarp::Router* r) override;

      bool
      HandleUpdateExitVerifyMessage(
          const llarp::routing::UpdateExitVerifyMessage* msg,
          llarp::Router* r) override;

      bool
      HandleTransferTrafficMessage(
          const llarp::routing::TransferTrafficMessage* msg,
          llarp::Router* r) override;

      bool
      HandleUpdateExitMessage(const llarp::routing::UpdateExitMessage* msg,
                              llarp::Router* r) override;

      bool
      HandleCloseExitMessage(const llarp::routing::CloseExitMessage* msg,
                             llarp::Router* r) override;
      bool
      HandleGrantExitMessage(const llarp::routing::GrantExitMessage* msg,
                             llarp::Router* r) override;
      bool
      HandleRejectExitMessage(const llarp::routing::RejectExitMessage* msg,
                              llarp::Router* r) override;

      bool
      HandleDataDiscardMessage(const llarp::routing::DataDiscardMessage* msg,
                               llarp::Router* r) override;

      bool
      HandlePathConfirmMessage(const llarp::routing::PathConfirmMessage* msg,
                               llarp::Router* r) override;

      bool
      HandlePathLatencyMessage(const llarp::routing::PathLatencyMessage* msg,
                               llarp::Router* r) override;

      bool
      HandlePathTransferMessage(const llarp::routing::PathTransferMessage* msg,
                                llarp::Router* r) override;

      bool
      HandleHiddenServiceFrame(
          const llarp::service::ProtocolFrame* frame) override;

      bool
      HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg);

      bool
      HandleDHTMessage(const llarp::dht::IMessage* msg,
                       llarp::Router* r) override;

      bool
      HandleRoutingMessage(llarp_buffer_t buf, llarp::Router* r);

      // handle data in upstream direction
      bool
      HandleUpstream(llarp_buffer_t X, const TunnelNonce& Y,
                     llarp::Router* r) override;

      // handle data in downstream direction
      bool
      HandleDownstream(llarp_buffer_t X, const TunnelNonce& Y,
                       llarp::Router* r) override;

      bool
      IsReady() const;

      // Is this deprecated?
      // nope not deprecated :^DDDD
      const PathID_t&
      TXID() const;

      RouterID
      Endpoint() const;

      PubKey
      EndpointPubKey() const;

      bool
      IsEndpoint(const RouterID& router, const PathID_t& path) const;

      const PathID_t&
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
      SendExitRequest(const llarp::routing::ObtainExitMessage* msg,
                      llarp::Router* r);

      bool
      SendExitClose(const llarp::routing::CloseExitMessage* msg,
                    llarp::Router* r);

     protected:
      llarp::routing::InboundMessageParser m_InboundMessageParser;

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
      PathContext(llarp::Router* router);
      ~PathContext();

      /// called from router tick function
      void
      ExpirePaths(llarp_time_t now);

      /// called from router tick function
      /// builds all paths we need to build at current tick
      void
      BuildPaths(llarp_time_t now);

      /// called from router tick function
      void
      TickPaths(llarp_time_t now);

      ///  track a path builder with this context
      void
      AddPathBuilder(Builder* set);

      void
      AllowTransit();

      void
      RejectTransit();

      bool
      AllowingTransit() const;

      bool
      HasTransitHop(const TransitHopInfo& info);

      bool
      HandleRelayCommit(const LR_CommitMessage* msg);

      void
      PutTransitHop(std::shared_ptr< TransitHop > hop);

      IHopHandler*
      GetByUpstream(const RouterID& id, const PathID_t& path);

      bool
      TransitHopPreviousIsRouter(const PathID_t& path, const RouterID& r);

      IHopHandler*
      GetPathForTransfer(const PathID_t& topath);

      IHopHandler*
      GetByDownstream(const RouterID& id, const PathID_t& path);

      PathSet*
      GetLocalPathSet(const PathID_t& id);

      routing::IMessageHandler*
      GetHandler(const PathID_t& id);

      bool
      ForwardLRCM(const RouterID& nextHop,
                  const std::array< EncryptedFrame, 8 >& frames);

      bool
      HopIsUs(const RouterID& k) const;

      bool
      HandleLRUM(const RelayUpstreamMessage* msg);

      bool
      HandleLRDM(const RelayDownstreamMessage* msg);

      void
      AddOwnPath(PathSet* set, Path* p);

      void
      RemovePathBuilder(Builder* ctx);

      void
      RemovePathSet(PathSet* set);

      using TransitHopsMap_t =
          std::multimap< PathID_t, std::shared_ptr< TransitHop > >;

      using SyncTransitMap_t = std::pair< util::Mutex, TransitHopsMap_t >;

      // maps path id -> pathset owner of path
      using OwnedPathsMap_t = std::map< PathID_t, PathSet* >;

      using SyncOwnedPathsMap_t = std::pair< util::Mutex, OwnedPathsMap_t >;

      llarp_threadpool*
      Worker();

      llarp::Crypto*
      Crypto();

      llarp::Logic*
      Logic();

      llarp::Router*
      Router();

      byte_t*
      EncryptionSecretKey();

      const byte_t*
      OurRouterID() const;

     private:
      llarp::Router* m_Router;
      SyncTransitMap_t m_TransitPaths;
      SyncTransitMap_t m_Paths;
      SyncOwnedPathsMap_t m_OurPaths;
      std::list< Builder* > m_PathBuilders;
      bool m_AllowTransit;
    };
  }  // namespace path
}  // namespace llarp

#endif
