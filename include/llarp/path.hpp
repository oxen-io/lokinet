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

      /// send routing message and increment sequence number
      virtual bool
      SendRoutingMessage(llarp::routing::IMessage* msg, llarp_router* r) = 0;

      // handle data in upstream direction
      virtual bool
      HandleUpstream(llarp_buffer_t X, const TunnelNonce& Y,
                     llarp_router* r) = 0;

      // handle data in downstream direction
      virtual bool
      HandleDownstream(llarp_buffer_t X, const TunnelNonce& Y,
                       llarp_router* r) = 0;

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
      Expired(llarp_time_t now) const;

      // send routing message when end of path
      bool
      SendRoutingMessage(llarp::routing::IMessage* msg, llarp_router* r);

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
      HandleHiddenServiceFrame(const llarp::service::ProtocolFrame* frame)
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
      typedef std::function< bool(const service::ProtocolFrame*) >
          DataHandlerFunc;

      HopList hops;

      llarp::service::Introduction intro;

      llarp_time_t buildStarted;
      PathStatus _status;

      Path(const std::vector< RouterContact >& routers);

      void
      SetBuildResultHook(BuildResultHookFunc func);

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
      EnterState(PathStatus st);

      llarp_time_t
      ExpireTime() const
      {
        return buildStarted + hops[0].lifetime;
      }

      bool
      Expired(llarp_time_t now) const;

      void
      Tick(llarp_time_t now, llarp_router* r);

      bool
      SendRoutingMessage(llarp::routing::IMessage* msg, llarp_router* r);

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

      const PathID_t&
      RXID() const;

      RouterID
      Upstream() const;

      std::string
      Name() const;

     protected:
      llarp::routing::InboundMessageParser m_InboundMessageParser;

     private:
      BuildResultHookFunc m_BuiltHook;
      DataHandlerFunc m_DataHandler;
      DropHandlerFunc m_DropHandler;
      CheckForDeadFunc m_CheckForDead;
      llarp_time_t m_LastLatencyTestTime = 0;
      uint64_t m_LastLatencyTestID       = 0;
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
      ExpirePaths();

      /// called from router tick function
      /// builds all paths we need to build at current tick
      void
      BuildPaths();

      /// called from router tick function
      void
      TickPaths();

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
      PutTransitHop(TransitHop* hop);

      IHopHandler*
      GetByUpstream(const RouterID& id, const PathID_t& path);

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

      typedef std::multimap< PathID_t, TransitHop* > TransitHopsMap_t;

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
