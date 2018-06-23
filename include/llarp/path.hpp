#ifndef LLARP_PATH_HPP
#define LLARP_PATH_HPP
#include <llarp/path.h>
#include <llarp/router.h>
#include <llarp/time.h>
#include <llarp/aligned.hpp>
#include <llarp/crypto.hpp>
#include <llarp/endpoint.hpp>
#include <llarp/messages/relay.hpp>
#include <llarp/messages/relay_commit.hpp>
#include <llarp/path_types.hpp>
#include <llarp/router_id.hpp>
#include <llarp/routing/handler.hpp>
#include <llarp/routing/message.hpp>

#include <list>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

#define DEFAULT_PATH_LIFETIME (10 * 60 * 1000)

namespace llarp
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
      out << " upstream=" << info.upstream << " downstream=" << info.downstream;
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
      return txID < other.txID || rxID < other.rxID || upstream < other.upstream
          || downstream < other.downstream;
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
    SendRoutingMessage(const llarp::routing::IMessage* msg,
                       llarp_router* r) = 0;

    // handle data in upstream direction
    virtual bool
    HandleUpstream(llarp_buffer_t X, const TunnelNonce& Y, llarp_router* r) = 0;

    // handle data in downstream direction
    virtual bool
    HandleDownstream(llarp_buffer_t X, const TunnelNonce& Y,
                     llarp_router* r) = 0;
  };

  struct TransitHop : public IHopHandler
  {
    TransitHop() = default;

    TransitHop(const TransitHop& other);

    TransitHopInfo info;
    SharedSecret pathKey;
    llarp_time_t started = 0;
    // 10 minutes default
    llarp_time_t lifetime = DEFAULT_PATH_LIFETIME;
    llarp_proto_version_t version;

    friend std::ostream&
    operator<<(std::ostream& out, const TransitHop& h)
    {
      return out << "[TransitHop " << h.info << " started=" << h.started
                 << " lifetime=" << h.lifetime << "]";
    }

    bool
    Expired(llarp_time_t now) const;

    bool
    SendRoutingMessage(const llarp::routing::IMessage* msg, llarp_router* r);

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
    llarp_rc router;
    // temp public encryption key
    SecretKey commkey;
    /// shared secret at this hop
    SharedSecret shared;
    /// next hop's router id
    RouterID upstream;
    /// nonce for key exchange
    TunnelNonce nonce;
    // lifetime
    llarp_time_t lifetime = DEFAULT_PATH_LIFETIME;

    ~PathHopConfig();
    PathHopConfig();
  };

  enum PathStatus
  {
    ePathBuilding,
    ePathEstablished,
    ePathTimeout,
    ePathExpired
  };

  /// A path we made
  struct Path : public IHopHandler, public llarp::routing::IMessageHandler
  {
    typedef std::vector< PathHopConfig > HopList;
    HopList hops;
    llarp_time_t buildStarted;
    PathStatus status;

    Path(llarp_path_hops* path);

    bool
    Expired(llarp_time_t now) const;

    bool
    SendRoutingMessage(const llarp::routing::IMessage* msg, llarp_router* r);

    bool
    HandleRoutingMessage(llarp_buffer_t buf, llarp_router* r);

    bool
    HandleHiddenServiceData(llarp_buffer_t buf);

    // handle data in upstream direction
    bool
    HandleUpstream(llarp_buffer_t X, const TunnelNonce& Y, llarp_router* r);

    // handle data in downstream direction
    bool
    HandleDownstream(llarp_buffer_t X, const TunnelNonce& Y, llarp_router* r);

    const PathID_t&
    TXID() const;

    const PathID_t&
    RXID() const;

    RouterID
    Upstream() const;

   protected:
    llarp::routing::InboundMessageParser m_InboundMessageParser;
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

    bool
    ForwardLRCM(const RouterID& nextHop, std::deque< EncryptedFrame >& frames);

    bool
    HopIsUs(const PubKey& k) const;

    bool
    HandleLRUM(const RelayUpstreamMessage* msg);

    bool
    HandleLRDM(const RelayDownstreamMessage* msg);

    void
    AddOwnPath(Path* p);

    typedef std::multimap< PathID_t, TransitHop* > TransitHopsMap_t;

    typedef std::pair< std::mutex, TransitHopsMap_t > SyncTransitMap_t;

    typedef std::map< PathID_t, Path* > OwnedPathsMap_t;

    typedef std::pair< std::mutex, OwnedPathsMap_t > SyncOwnedPathsMap_t;

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

    bool m_AllowTransit;
  };
}  // namespace llarp

#endif
