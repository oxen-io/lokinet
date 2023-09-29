#pragma once

#include <llarp/router_id.hpp>
#include <llarp/util/thread/threading.hpp>

#include <chrono>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <list>

struct llarp_dht_context;

namespace llarp
{
  class NodeDB;
  class EventLoop;

  namespace dht
  {
    struct AbstractDHTMessageHandler;
  }  // namespace dht

  namespace service
  {
    struct Context;
  }  // namespace service

  struct Contacts;
  struct LinkManager;
  struct RouterContact;

  enum class RCRequestResult
  {
    Success,
    InvalidRouter,
    RouterNotFound,
    BadRC
  };

  using RCRequestCallback =
      std::function<void(const RouterID&, const RouterContact* const, const RCRequestResult)>;

  struct RCLookupHandler
  {
   public:
    using worker_func = std::function<void(std::function<void()>)>;
    using callback_que = std::list<RCRequestCallback>;

    ~RCLookupHandler() = default;

    void
    add_valid_router(const RouterID& router) EXCLUDES(_mutex);

    void
    remove_valid_router(const RouterID& router) EXCLUDES(_mutex);

    void
    set_router_whitelist(
        const std::vector<RouterID>& whitelist,
        const std::vector<RouterID>& greylist,
        const std::vector<RouterID>& greenlist

        ) EXCLUDES(_mutex);

    bool
    has_received_whitelist() const;

    void
    get_rc(const RouterID& router, RCRequestCallback callback, bool forceLookup = false)
        EXCLUDES(_mutex);

    bool
    is_path_allowed(const RouterID& remote) const EXCLUDES(_mutex);

    bool
    is_session_allowed(const RouterID& remote) const EXCLUDES(_mutex);

    bool
    is_grey_listed(const RouterID& remote) const EXCLUDES(_mutex);

    // "greenlist" = new routers (i.e. "green") that aren't fully funded yet
    bool
    is_green_listed(const RouterID& remote) const EXCLUDES(_mutex);

    // registered just means that there is at least an operator stake, but doesn't require the node
    // be fully funded, active, or not decommed.  (In other words: it is any of the white, grey, or
    // green list).
    bool
    is_registered(const RouterID& remote) const EXCLUDES(_mutex);

    bool
    check_rc(const RouterContact& rc) const;

    bool
    get_random_whitelist_router(RouterID& router) const EXCLUDES(_mutex);

    bool
    check_renegotiate_valid(RouterContact newrc, RouterContact oldrc);

    void
    periodic_update(llarp_time_t now);

    void
    explore_network();

    size_t
    num_strict_connect_routers() const;

    void
    init(
        std::shared_ptr<Contacts> contacts,
        std::shared_ptr<NodeDB> nodedb,
        std::shared_ptr<EventLoop> loop,
        worker_func dowork,
        LinkManager* linkManager,
        service::Context* hiddenServiceContext,
        const std::unordered_set<RouterID>& strictConnectPubkeys,
        const std::set<RouterContact>& bootstrapRCList,
        bool useWhitelist_arg,
        bool isServiceNode_arg);

    std::unordered_set<RouterID>
    whitelist() const
    {
      util::Lock lock{_mutex};
      return router_whitelist;
    }

   private:
    void
    handle_dht_lookup_result(RouterID remote, const std::vector<RouterContact>& results);

    bool
    has_pending_lookup(RouterID remote) const EXCLUDES(_mutex);

    bool
    is_remote_in_bootstrap(const RouterID& remote) const;

    void
    finalize_request(const RouterID& router, const RouterContact* const rc, RCRequestResult result)
        EXCLUDES(_mutex);

    mutable util::Mutex _mutex;  // protects pendingCallbacks, whitelistRouters

    std::shared_ptr<Contacts> contacts = nullptr;
    std::shared_ptr<NodeDB> node_db;
    std::shared_ptr<EventLoop> loop;
    worker_func work_func = nullptr;
    service::Context* hidden_service_context = nullptr;
    LinkManager* link_manager = nullptr;

    /// explicit whitelist of routers we will connect to directly (not for
    /// service nodes)
    std::unordered_set<RouterID> strict_connect_pubkeys;

    std::set<RouterContact> bootstrap_rc_list;
    std::unordered_set<RouterID> boostrap_rid_list;

    std::unordered_map<RouterID, callback_que> pending_callbacks GUARDED_BY(_mutex);

    bool useWhitelist = false;
    bool isServiceNode = false;

    // whitelist = active routers
    std::unordered_set<RouterID> router_whitelist GUARDED_BY(_mutex);
    // greylist = fully funded, but decommissioned routers
    std::unordered_set<RouterID> router_greylist GUARDED_BY(_mutex);
    // greenlist = registered but not fully-staked routers
    std::unordered_set<RouterID> router_greenlist GUARDED_BY(_mutex);

    using TimePoint = std::chrono::steady_clock::time_point;
    std::unordered_map<RouterID, TimePoint> router_lookup_times;
  };

}  // namespace llarp
