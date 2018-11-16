#ifndef LLARP_PATH_HPP
#define LLARP_PATH_HPP
#include <llarp/router.h>
#include <llarp/time.h>
#include <llarp/aligned.hpp>
#include <llarp/crypto.hpp>
#include <llarp/dht.hpp>
#include <llarp/messages/relay.hpp>
#include <llarp/messages/relay_commit.hpp>
#include <llarp/path_types.hpp>
#include <llarp/pathset.hpp>
#include <llarp/pathbuilder.hpp>
#include <llarp/router_id.hpp>
#include <llarp/routing/handler.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/service/Intro.hpp>
#include <llarp/threading.hpp>

#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

#define MAXHOPS (8)
#define DEFAULT_PATH_LIFETIME (10 * 60 * 1000)
#define PATH_BUILD_TIMEOUT (30 * 1000)
#define MESSAGE_PAD_SIZE (512)
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
                         llarp_router* r) = 0;

      // handle data in upstream direction
      virtual bool
      HandleUpstream(llarp_buffer_t X, const TunnelNonce& Y,
                     llarp_router* r) = 0;

      // handle data in downstream direction
      virtual bool
      HandleDownstream(llarp_buffer_t X, const TunnelNonce& Y,
                       llarp_router* r) = 0;

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
      SendRoutingMessage(const llarp::routing::IMessage* msg, llarp_router* r);

      // handle routing message when end of path
      bool
      HandleRoutingMessage(const llarp::routing::IMessage* msg,
                           llarp_router* r);

      bool
      HandleDataDiscardMessage(const llarp::routing::DataDiscardMessage* msg,
                               llarp_router* r);

      bool
      HandlePathConfirmMessage(const llarp::routing::PathConfirmMessage* msg,
                               llarp_router* r);
      bool
      HandlePathTransferMessage(const llarp::routing::PathTransferMessage* msg,
                                llarp_router* r);
      bool
      HandlePathLatencyMessage(const llarp::routing::PathLatencyMessage* msg,
                               llarp_router* r);

      bool
      HandleObtainExitMessage(const llarp::routing::ObtainExitMessage* msg,
                              llarp_router* r);

      bool
      HandleUpdateExitVerifyMessage(
          const llarp::routing::UpdateExitVerifyMessage* msg, llarp_router* r);

      bool
      HandleTransferTrafficMessage(
          const llarp::routing::TransferTrafficMessage* msg, llarp_router* r);

      bool
      HandleUpdateExitMessage(const llarp::routing::UpdateExitMessage* msg,
                              llarp_router* r);

      bool
      HandleGrantExitMessage(const llarp::routing::GrantExitMessage* msg,
                             llarp_router* r);
      bool
      HandleRejectExitMessage(const llarp::routing::RejectExitMessage* msg,
                              llarp_router* r);

      bool
      HandleCloseExitMessage(const llarp::routing::CloseExitMessage* msg,
                             llarp_router* r);

      bool
      HandleHiddenServiceFrame(__attribute__((unused))
                               const llarp::service::ProtocolFrame* frame)
      {
        /// TODO: implement me
        llarp::LogWarn("Got hidden service data on transit hop");
        return false;
      }

      bool
      HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg);

      bool
      HandleDHTMessage(const llarp::dht::IMessage* msg, llarp_router* r);

      // handle data in upstream direction
      bool
      HandleUpstream(llarp_buffer_t X, const TunnelNonce& Y, llarp_router* r);

      // handle data in downstream direction
      bool
      HandleDownstream(llarp_buffer_t X, const TunnelNonce& Y, llarp_router* r);
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
      typedef std::function< void(Path*) > BuildResultHookFunc;
      typedef std::function< bool(Path*, llarp_time_t) > CheckForDeadFunc;
      typedef std::function< bool(Path*, const PathID_t&, uint64_t) >
          DropHandlerFunc;
      typedef std::vector< PathHopConfig > HopList;
      typedef std::function< bool(Path*, const service::ProtocolFrame*) >
          DataHandlerFunc;
      typedef std::function< bool(Path*) > ExitUpdatedFunc;
      typedef std::function< bool(Path*) > ExitClosedFunc;
      typedef std::function< bool(Path*, llarp_buffer_t) >
          ExitTrafficHandlerFunc;
      /// (path, backoff) backoff is 0 on success
      typedef std::function< bool(Path*, llarp_time_t) > ObtainedExitHandler;

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

      bool
      MarkActive(llarp_time_t now)
      {
        m_LastRecvMessage = now;
      }

      bool
      SupportsRoles(PathRole roles) const
      {
        return (_role & roles) == roles;
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
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt = 5000) const
      {
        return now >= (ExpireTime() - dlt);
      }

      bool
      Expired(llarp_time_t now) const;

      void
      Tick(llarp_time_t now, llarp_router* r);

      bool
      SendRoutingMessage(const llarp::routing::IMessage* msg, llarp_router* r);

      bool
      HandleObtainExitMessage(const llarp::routing::ObtainExitMessage* msg,
                              llarp_router* r);

      bool
      HandleUpdateExitVerifyMessage(
          const llarp::routing::UpdateExitVerifyMessage* msg, llarp_router* r);

      bool
      HandleTransferTrafficMessage(
          const llarp::routing::TransferTrafficMessage* msg, llarp_router* r);

      bool
      HandleUpdateExitMessage(const llarp::routing::UpdateExitMessage* msg,
                              llarp_router* r);

      bool
      HandleCloseExitMessage(const llarp::routing::CloseExitMessage* msg,
                             llarp_router* r);
      bool
      HandleRejectExitMessagge(const llarp::routing::RejectExitMessage* msg,
                               llarp_router* r);

      bool
      HandleGrantExitMessage(const llarp::routing::GrantExitMessage* msg,
                             llarp_router* r);
      bool
      HandleRejectExitMessage(const llarp::routing::RejectExitMessage* msg,
                              llarp_router* r);

      bool
      HandleDataDiscardMessage(const llarp::routing::DataDiscardMessage* msg,
                               llarp_router* r);

      bool
      HandlePathConfirmMessage(const llarp::routing::PathConfirmMessage* msg,
                               llarp_router* r);

      bool
      HandlePathLatencyMessage(const llarp::routing::PathLatencyMessage* msg,
                               llarp_router* r);

      bool
      HandlePathTransferMessage(const llarp::routing::PathTransferMessage* msg,
                                llarp_router* r);

      bool
      HandleHiddenServiceFrame(const llarp::service::ProtocolFrame* frame);

      bool
      HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg);

      bool
      HandleDHTMessage(const llarp::dht::IMessage* msg, llarp_router* r);

      bool
      HandleRoutingMessage(llarp_buffer_t buf, llarp_router* r);

      // handle data in upstream direction
      bool
      HandleUpstream(llarp_buffer_t X, const TunnelNonce& Y, llarp_router* r);

      // handle data in downstream direction
      bool
      HandleDownstream(llarp_buffer_t X, const TunnelNonce& Y, llarp_router* r);

      bool
      IsReady() const;

      // Is this deprecated?
      // nope not deprecated :^DDDD
      const PathID_t&
      TXID() const;

      RouterID
      Endpoint() const;

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
                      llarp_router* r);

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
      PathContext(llarp_router* router);
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
      HopIsUs(const PubKey& k) const;

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

      typedef std::multimap< PathID_t, std::shared_ptr< TransitHop > >
          TransitHopsMap_t;

      typedef std::pair< util::Mutex, TransitHopsMap_t > SyncTransitMap_t;

      // maps path id -> pathset owner of path
      typedef std::map< PathID_t, PathSet* > OwnedPathsMap_t;

      typedef std::pair< util::Mutex, OwnedPathsMap_t > SyncOwnedPathsMap_t;

      llarp_threadpool*
      Worker();

      llarp_crypto*
      Crypto();

      llarp_logic*
      Logic();

      llarp_router*
      Router();

      byte_t*
      EncryptionSecretKey();

      const byte_t*
      OurRouterID() const;

     private:
      llarp_router* m_Router;
      SyncTransitMap_t m_TransitPaths;
      SyncTransitMap_t m_Paths;
      SyncOwnedPathsMap_t m_OurPaths;
      std::list< Builder* > m_PathBuilders;
      bool m_AllowTransit;
    };
  }  // namespace path
}  // namespace llarp

#endif
