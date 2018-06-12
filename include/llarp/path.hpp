#ifndef LLARP_PATH_HPP
#define LLARP_PATH_HPP
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

    PathID_t rxID;
    PathID_t txID;
    RouterID upstream;
    RouterID downstream;

    friend std::ostream&
    operator<<(std::ostream& out, const TransitHopInfo& info)
    {
      out << "<Transit Hop rxid=" << info.rxID << " txid=" << info.txID;
      out << " upstream=" << info.upstream << " downstream=" << info.downstream;
      return out << ">";
    }

    bool
    operator==(const TransitHopInfo& other) const
    {
      return rxID == other.rxID && txID == other.txID
          && upstream == other.upstream && downstream == other.downstream;
    }

    bool
    operator!=(const TransitHopInfo& other) const
    {
      return !(*this == other);
    }

    struct Hash
    {
      std::size_t
      operator()(TransitHopInfo const& a) const
      {
        std::size_t idx0, idx1, idx2, idx3;
        memcpy(&idx0, a.upstream, sizeof(std::size_t));
        memcpy(&idx1, a.downstream, sizeof(std::size_t));
        memcpy(&idx2, a.rxID, sizeof(std::size_t));
        memcpy(&idx3, a.txID, sizeof(std::size_t));
        return idx0 ^ idx1 ^ idx2 ^ idx3;
      }
    };
  };

  struct TransitHop
  {
    SharedSecret rxKey;
    SharedSecret txKey;
    llarp_time_t started;
    llarp_proto_version_t version;
  };

  struct PathHopConfig
  {
    /// path id
    PathID_t txID;
    /// router identity key
    PubKey encryptionKey;
    /// shared secret at this hop
    SharedSecret shared;
    /// nonce for key exchange
    TunnelNonce nonce;
  };

  struct Path
  {
    typedef std::vector< PathHopConfig > HopList;
    HopList hops;
    llarp_time_t buildStarted;
  };

  template < typename User >
  struct AsyncPathKeyExchangeContext
  {
    Path path;
    typedef void (*Handler)(AsyncPathKeyExchangeContext*);
    User* user               = nullptr;
    Handler result           = nullptr;
    const byte_t* secretkey  = nullptr;
    size_t idx               = 0;
    llarp_threadpool* worker = nullptr;
    llarp_path_dh_func dh    = nullptr;

    static void
    GenerateNextKey(void* user)
    {
      AsyncPathKeyExchangeContext< User >* ctx =
          static_cast< AsyncPathKeyExchangeContext< User >* >(user);

      auto& hop = ctx->path.hops[ctx->idx];
      ctx->dh(hop.shared, hop.encryptionKey, hop.nonce, ctx->secretkey);
      ++ctx->idx;
      if(ctx->idx < ctx.path.hops.size())
      {
        llarp_threadpool_queue_job(ctx->worker, {ctx, &GenerateNextKey});
      }
      else
      {
        ctx->Done();
      }
    }

    AsyncPathKeyExchangeContext(const byte_t* secret, llarp_crypto* crypto)
        : secretkey(secret), dh(crypto->dh_client)
    {
    }

    void
    Done()
    {
      idx = 0;
      result(this);
    }

    /// Generate all keys asynchronously and call hadler when done
    void
    AsyncGenerateKeys(llarp_threadpool* pool, User* u, Handler func) const
    {
      user   = u;
      result = func;
      worker = pool;
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

    void
    AllowTransit();
    void
    RejectTransit();

    bool
    HasTransitHop(const TransitHopInfo& info);

    bool
    HandleRelayCommit(const LR_CommitMessage* msg);

    bool
    HandleRelayAck(const LR_AckMessage* msg);

    typedef std::unordered_map< TransitHopInfo, TransitHop,
                                TransitHopInfo::Hash >
        TransitHopsMap_t;

   private:
    llarp_router* m_Router;
    std::mutex m_TransitPathsMutex;
    TransitHopsMap_t m_TransitPaths;
    bool m_AllowTransit;
  };
}

#endif