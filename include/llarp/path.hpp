#ifndef LLARP_PATH_HPP
#define LLARP_PATH_HPP
#include <llarp/path.h>
#include <llarp/router.h>
#include <llarp/time.h>
#include <llarp/aligned.hpp>
#include <llarp/crypto.hpp>
#include <llarp/messages/relay_ack.hpp>
#include <llarp/messages/relay_commit.hpp>
#include <llarp/path_types.hpp>
#include <llarp/router_id.hpp>

#include <mutex>
#include <unordered_map>
#include <vector>

namespace llarp
{
  struct TransitHopInfo
  {
    TransitHopInfo() = default;
    TransitHopInfo(const RouterID& down, const LR_CommitRecord& record);

    PathID_t pathID;
    RouterID upstream;
    RouterID downstream;

    friend std::ostream&
    operator<<(std::ostream& out, const TransitHopInfo& info)
    {
      out << "<Transit Hop id=" << info.pathID;
      out << " upstream=" << info.upstream << " downstream=" << info.downstream;
      return out << ">";
    }

    bool
    operator==(const TransitHopInfo& other) const
    {
      return pathID == other.pathID && upstream == other.upstream
          && downstream == other.downstream;
    }

    bool
    operator!=(const TransitHopInfo& other) const
    {
      return !(*this == other);
    }

    bool
    operator<(const TransitHopInfo& other) const
    {
      return pathID < other.pathID || upstream < other.upstream
          || downstream < other.downstream;
    }

    struct Hash
    {
      std::size_t
      operator()(TransitHopInfo const& a) const
      {
        std::size_t idx0, idx1, idx2;
        memcpy(&idx0, a.upstream, sizeof(std::size_t));
        memcpy(&idx1, a.downstream, sizeof(std::size_t));
        memcpy(&idx2, a.pathID, sizeof(std::size_t));
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

  struct TransitHop
  {
    TransitHop() = default;

    TransitHopInfo info;
    SharedSecret pathKey;
    llarp_time_t started;
    // 10 minutes default
    llarp_time_t lifetime = 360000;
    llarp_proto_version_t version;

    bool
    Expired(llarp_time_t now) const;
  };

  /// configuration for a single hop when building a path
  struct PathHopConfig
  {
    /// path id
    PathID_t pathID;
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

    ~PathHopConfig();
    PathHopConfig();
  };

  struct Path
  {
    typedef std::vector< PathHopConfig > HopList;
    HopList hops;
    llarp_time_t buildStarted;
    Path(llarp_path_hops* path);
  };

  template < typename User >
  struct AsyncPathKeyExchangeContext
  {
    Path* path = nullptr;
    typedef void (*Handler)(AsyncPathKeyExchangeContext< User >*);
    User* user               = nullptr;
    Handler result           = nullptr;
    size_t idx               = 0;
    llarp_threadpool* worker = nullptr;
    llarp_logic* logic       = nullptr;
    llarp_crypto* crypto     = nullptr;
    LR_CommitMessage LRCM;

    static void
    HandleDone(void* user)
    {
      AsyncPathKeyExchangeContext< User >* ctx =
          static_cast< AsyncPathKeyExchangeContext< User >* >(user);
      ctx->result(ctx);
      delete ctx;
    }

    static void
    GenerateNextKey(void* user)
    {
      AsyncPathKeyExchangeContext< User >* ctx =
          static_cast< AsyncPathKeyExchangeContext< User >* >(user);

      auto& hop = ctx->path->hops[ctx->idx];
      // generate key
      ctx->crypto->encryption_keygen(hop.commkey);
      // do key exchange
      if(!ctx->crypto->dh_client(hop.shared, hop.router.enckey, hop.nonce,
                                 hop.commkey))
      {
        llarp::Error("Failed to generate shared key for path build");
        delete ctx;
        return;
      }
      // randomize hop's path id
      hop.pathID.Randomize();

      LR_CommitRecord record;

      auto& frame = ctx->LRCM.frames[ctx->idx];
      ++ctx->idx;
      if(ctx->idx < ctx->path->hops.size())
      {
        hop.upstream = ctx->path->hops[ctx->idx].router.pubkey;
      }
      else
      {
        hop.upstream = hop.router.pubkey;
      }
      // generate record
      if(!record.BEncode(frame.Buffer()))
      {
        // failed to encode?
        llarp::Error("Failed to generate Commit Record");
        delete ctx;
        return;
      }

      if(ctx->idx < ctx->path->hops.size())
      {
        // next hop
        llarp_threadpool_queue_job(ctx->worker, {ctx, &GenerateNextKey});
      }
      else
      {
        // farthest hop
        llarp_logic_queue_job(ctx->logic, {ctx, &HandleDone});
      }
    }

    AsyncPathKeyExchangeContext(llarp_crypto* c) : crypto(c)
    {
    }

    /// Generate all keys asynchronously and call hadler when done
    void
    AsyncGenerateKeys(Path* p, llarp_logic* l, llarp_threadpool* pool, User* u,
                      Handler func)
    {
      path   = p;
      logic  = l;
      user   = u;
      result = func;
      worker = pool;

      for(size_t idx = 0; idx < MAXHOPS; ++idx)
      {
        LRCM.frames.emplace_back(256);
        LRCM.frames.back().Randomize();
      }
      llarp_threadpool_queue_job(pool, {this, &GenerateNextKey});
    }
  };

  enum PathBuildStatus
  {
    ePathBuildSuccess,
    ePathBuildTimeout,
    ePathBuildReject
  };

  /// path selection algorithm
  struct IPathSelectionAlgorithm
  {
    virtual ~IPathSelectionAlgorithm(){};
    /// select full path given an empty hop list to end at target
    virtual bool
    SelectFullPathTo(Path::HopList& hops, const RouterID& target) = 0;

    /// report to path builder the result of a path build
    /// can be used to "improve" path building algoirthm in the
    /// future
    virtual void
    ReportPathBuildStatus(const Path::HopList& hops, const RouterID& target,
                          PathBuildStatus status){};
  };

  class PathBuildJob
  {
   public:
    PathBuildJob(llarp_router* router, IPathSelectionAlgorithm* selector);
    ~PathBuildJob();

    void
    Start();

   private:
    typedef AsyncPathKeyExchangeContext< PathBuildJob > KeyExchanger;

    LR_CommitMessage*
    BuildLRCM();

    static void
    KeysGenerated(KeyExchanger* ctx);

    llarp_router* router;
    IPathSelectionAlgorithm* m_HopSelector;
    KeyExchanger m_KeyExchanger;
  };

  /// a pool of paths for a hidden service
  struct PathPool
  {
    PathPool(llarp_router* router);
    ~PathPool();

    /// build a new path to a router by identity key
    PathBuildJob*
    BuildNewPathTo(const RouterID& router);
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
    PutTransitHop(const TransitHop& hop);

    bool
    ForwardLRCM(const RouterID& nextHop, std::deque< EncryptedFrame >& frames);

    void
    ForwradLRUM(const PathID_t& id, const RouterID& from, llarp_buffer_t X,
                const TunnelNonce& nonce);

    void
    ForwradLRDM(const PathID_t& id, const RouterID& from, llarp_buffer_t X,
                const TunnelNonce& nonce);

    bool
    HopIsUs(const PubKey& k) const;

    typedef std::unordered_multimap< PathID_t, TransitHop, PathIDHash >
        TransitHopsMap_t;

    typedef std::pair< std::mutex, TransitHopsMap_t > SyncTransitMap_t;

    llarp_threadpool*
    Worker();

    llarp_crypto*
    Crypto();

    llarp_logic*
    Logic();

    byte_t*
    EncryptionSecretKey();

    const byte_t*
    OurRouterID() const;

   private:
    llarp_router* m_Router;
    SyncTransitMap_t m_TransitPaths;

    bool m_AllowTransit;
  };
}  // namespace llarp

#endif
