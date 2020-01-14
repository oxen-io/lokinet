#ifndef LLARP_RC_LOOKUP_HANDLER_HPP
#define LLARP_RC_LOOKUP_HANDLER_HPP

#include <router/i_rc_lookup_handler.hpp>

#include <util/thread/threading.hpp>
#include <util/thread/thread_pool.hpp>

#include <unordered_map>
#include <set>
#include <list>

struct llarp_nodedb;
struct llarp_dht_context;

namespace llarp
{
  namespace service
  {
    struct Context;

  }  // namespace service

  struct ILinkManager;

  struct RCLookupHandler final : public I_RCLookupHandler
  {
   public:
    using CallbacksQueue = std::list< RCRequestCallback >;

    ~RCLookupHandler() override = default;

    void
    AddValidRouter(const RouterID &router) override LOCKS_EXCLUDED(_mutex);

    void
    RemoveValidRouter(const RouterID &router) override LOCKS_EXCLUDED(_mutex);

    void
    SetRouterWhitelist(const std::vector< RouterID > &routers) override
        LOCKS_EXCLUDED(_mutex);

    void
    GetRC(const RouterID &router, RCRequestCallback callback,
          bool forceLookup = false) override LOCKS_EXCLUDED(_mutex);

    bool
    RemoteIsAllowed(const RouterID &remote) const override
        LOCKS_EXCLUDED(_mutex);

    bool
    CheckRC(const RouterContact &rc) const override;

    bool
    GetRandomWhitelistRouter(RouterID &router) const override
        LOCKS_EXCLUDED(_mutex);

    bool
    CheckRenegotiateValid(RouterContact newrc, RouterContact oldrc) override;

    void
    PeriodicUpdate(llarp_time_t now) override;

    void
    ExploreNetwork() override;

    size_t
    NumberOfStrictConnectRouters() const override;

    void
    Init(llarp_dht_context *dht, llarp_nodedb *nodedb,
         std::shared_ptr< llarp::thread::ThreadPool > threadpool,
         ILinkManager *linkManager, service::Context *hiddenServiceContext,
         const std::set< RouterID > &strictConnectPubkeys,
         const std::set< RouterContact > &bootstrapRCList,
         bool useWhitelist_arg, bool isServiceNode_arg);

   private:
    void
    HandleDHTLookupResult(RouterID remote,
                          const std::vector< RouterContact > &results);

    bool
    HavePendingLookup(RouterID remote) const LOCKS_EXCLUDED(_mutex);

    bool
    RemoteInBootstrap(const RouterID &remote) const;

    void
    FinalizeRequest(const RouterID &router, const RouterContact *const rc,
                    RCRequestResult result) LOCKS_EXCLUDED(_mutex);

    mutable util::Mutex _mutex;  // protects pendingCallbacks, whitelistRouters

    llarp_dht_context *_dht                                  = nullptr;
    llarp_nodedb *_nodedb                                    = nullptr;
    std::shared_ptr< llarp::thread::ThreadPool > _threadpool = nullptr;
    service::Context *_hiddenServiceContext                  = nullptr;
    ILinkManager *_linkManager                               = nullptr;

    /// explicit whitelist of routers we will connect to directly (not for
    /// service nodes)
    std::set< RouterID > _strictConnectPubkeys;

    std::set< RouterContact > _bootstrapRCList;
    std::set< RouterID > _bootstrapRouterIDList;

    std::unordered_map< RouterID, CallbacksQueue, RouterID::Hash >
        pendingCallbacks GUARDED_BY(_mutex);

    bool useWhitelist  = false;
    bool isServiceNode = false;

    std::set< RouterID > whitelistRouters GUARDED_BY(_mutex);
  };

}  // namespace llarp

#endif  // LLARP_RC_LOOKUP_HANDLER_HPP
