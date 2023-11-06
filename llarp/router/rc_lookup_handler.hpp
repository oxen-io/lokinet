#pragma once

#include <llarp/router_contact.hpp>
#include <llarp/router_id.hpp>
#include <llarp/util/thread/threading.hpp>

#include <chrono>
#include <list>
#include <set>
#include <unordered_map>
#include <unordered_set>

struct llarp_dht_context;

namespace llarp
{
  class NodeDB;
  struct Router;
  class EventLoop;

  namespace service
  {
    struct Context;
  }  // namespace service

  struct Contacts;
  struct LinkManager;

  enum class RCRequestResult
  {
    Success,
    InvalidRouter,
    RouterNotFound,
    BadRC
  };

  using RCRequestCallback =
      std::function<void(const RouterID&, std::optional<RemoteRC>, bool success)>;

  struct RCLookupHandler
  {
   public:
    ~RCLookupHandler() = default;

    void
    add_valid_router(const RouterID& router);

    void
    remove_valid_router(const RouterID& router);

    void
    set_router_whitelist(
        const std::vector<RouterID>& whitelist,
        const std::vector<RouterID>& greylist,
        const std::vector<RouterID>& greenlist);

    bool
    has_received_whitelist() const;

    void
    get_rc(const RouterID& router, RCRequestCallback callback, bool forceLookup = false);

    bool
    is_path_allowed(const RouterID& remote) const;

    bool
    is_session_allowed(const RouterID& remote) const;

    bool
    is_grey_listed(const RouterID& remote) const;

    // "greenlist" = new routers (i.e. "green") that aren't fully funded yet
    bool
    is_green_listed(const RouterID& remote) const;

    // registered just means that there is at least an operator stake, but doesn't require the node
    // be fully funded, active, or not decommed.  (In other words: it is any of the white, grey, or
    // green list).
    bool
    is_registered(const RouterID& remote) const;

    bool
    check_rc(const RemoteRC& rc) const;

    bool
    get_random_whitelist_router(RouterID& router) const;

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
        std::function<void(std::function<void()>)> dowork,
        LinkManager* linkManager,
        service::Context* hiddenServiceContext,
        const std::unordered_set<RouterID>& strictConnectPubkeys,
        const std::set<RemoteRC>& bootstrapRCList,
        bool isServiceNode_arg);

    std::unordered_set<RouterID>
    whitelist() const;

   private:
    bool
    is_remote_in_bootstrap(const RouterID& remote) const;

    std::shared_ptr<Contacts> contacts = nullptr;
    std::shared_ptr<NodeDB> node_db;
    std::shared_ptr<EventLoop> loop;
    std::function<void(std::function<void()>)> work_func = nullptr;
    service::Context* hidden_service_context = nullptr;
    LinkManager* link_manager = nullptr;
    Router* router;

    /// explicit whitelist of routers we will connect to directly (not for
    /// service nodes)
    std::unordered_set<RouterID> strict_connect_pubkeys;

    std::set<RemoteRC> bootstrap_rc_list;
    std::unordered_set<RouterID> boostrap_rid_list;

    // Now that all calls are made through the event loop, any access to these
    // booleans is not guarded by a mutex
    std::atomic<bool> isServiceNode = false;

    // whitelist = active routers
    std::unordered_set<RouterID> router_whitelist;
    // greylist = fully funded, but decommissioned routers
    std::unordered_set<RouterID> router_greylist;
    // greenlist = registered but not fully-staked routers
    std::unordered_set<RouterID> router_greenlist;
  };

}  // namespace llarp
