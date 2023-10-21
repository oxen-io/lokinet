#include "rc_lookup_handler.hpp"

#include "router.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/link/contacts.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/service/context.hpp>
#include <llarp/util/types.hpp>

#include <functional>
#include <iterator>

namespace llarp
{
  void
  RCLookupHandler::add_valid_router(const RouterID& rid)
  {
    router->loop()->call([this, rid]() { router_whitelist.insert(rid); });
  }

  void
  RCLookupHandler::remove_valid_router(const RouterID& rid)
  {
    router->loop()->call([this, rid]() { router_whitelist.erase(rid); });
  }

  static void
  loadColourList(std::unordered_set<RouterID>& beigelist, const std::vector<RouterID>& new_beige)
  {
    beigelist.clear();
    beigelist.insert(new_beige.begin(), new_beige.end());
  }

  void
  RCLookupHandler::set_router_whitelist(
      const std::vector<RouterID>& whitelist,
      const std::vector<RouterID>& greylist,
      const std::vector<RouterID>& greenlist)
  {
    if (whitelist.empty())
      return;

    router->loop()->call([this, whitelist, greylist, greenlist]() {
      loadColourList(router_whitelist, whitelist);
      loadColourList(router_greylist, greylist);
      loadColourList(router_greenlist, greenlist);
      LogInfo("lokinet service node list now has ", router_whitelist.size(), " active routers");
    });
  }

  bool
  RCLookupHandler::has_received_whitelist() const
  {
    return router->loop()->call_get([this]() { return not router_whitelist.empty(); });
  }

  std::unordered_set<RouterID>
  RCLookupHandler::whitelist() const
  {
    return router->loop()->call_get([this]() { return router_whitelist; });
  }

  void
  RCLookupHandler::get_rc(const RouterID& rid, RCRequestCallback callback, bool forceLookup)
  {
    RemoteRC remoteRC;

    if (not forceLookup)
    {
      if (const auto maybe = node_db->get_rc(rid); maybe.has_value())
      {
        remoteRC = *maybe;

        if (callback)
        {
          callback(rid, remoteRC, true);
        }

        return;
      }
    }

    auto lookup_cb = [this, callback, rid](RemoteRC rc, bool success) mutable {
      auto& r = link_manager->router();

      if (not success)
      {
        if (callback)
          callback(rid, std::nullopt, false);
        return;
      }

      r.node_db()->put_rc_if_newer(rc);
      if (callback)
        callback(rc.router_id(), rc, true);
    };

    // TODO: RC fetching and gossiping in general is being refactored, and the old method
    //       of look it up over a path or directly but using the same method but called
    //       differently is going away.  It's a mess.  The below will do a lookup via a path,
    //       relays will need a different implementation TBD.
    if (!isServiceNode)
      hidden_service_context->GetDefault()->lookup_router(rid, std::move(lookup_cb));
  }

  bool
  RCLookupHandler::is_grey_listed(const RouterID& remote) const
  {
    if (strict_connect_pubkeys.size() && strict_connect_pubkeys.count(remote) == 0
        && !is_remote_in_bootstrap(remote))
    {
      return false;
    }

    if (not isServiceNode)
      return false;

    return router->loop()->call_get([this, remote]() { return router_greylist.count(remote); });
  }

  bool
  RCLookupHandler::is_green_listed(const RouterID& remote) const
  {
    return router->loop()->call_get([this, remote]() { return router_greenlist.count(remote); });
  }

  bool
  RCLookupHandler::is_registered(const RouterID& rid) const
  {
    return router->loop()->call_get([this, rid]() {
      return router_whitelist.count(rid) || router_greylist.count(rid)
          || router_greenlist.count(rid);
    });
  }

  bool
  RCLookupHandler::is_path_allowed(const RouterID& rid) const
  {
    return router->loop()->call_get([this, rid]() {
      if (strict_connect_pubkeys.size() && strict_connect_pubkeys.count(rid) == 0
          && !is_remote_in_bootstrap(rid))
      {
        return false;
      }

      if (not isServiceNode)
        return true;

      return router_whitelist.count(rid) != 0;
    });
  }

  bool
  RCLookupHandler::is_session_allowed(const RouterID& rid) const
  {
    return router->loop()->call_get([this, rid]() {
      if (strict_connect_pubkeys.size() && strict_connect_pubkeys.count(rid) == 0
          && !is_remote_in_bootstrap(rid))
      {
        return false;
      }

      if (not isServiceNode)
        return true;

      return router_whitelist.count(rid) or router_greylist.count(rid);
    });
  }

  bool
  RCLookupHandler::check_rc(const RemoteRC& rc) const
  {
    if (not is_session_allowed(rc.router_id()))
    {
      contacts->delete_rc_node_async(dht::Key_t{rc.router_id()});
      return false;
    }

    if (not rc.verify())
    {
      log::info(link_cat, "Invalid RC (rid: {})", rc.router_id());
      return false;
    }

    // update nodedb if required
    if (rc.is_public_router())
    {
      log::info(link_cat, "Adding or updating RC (rid: {}) to nodeDB and DHT", rc.router_id());
      node_db->put_rc_if_newer(rc);
      contacts->put_rc_node_async(rc);
    }

    return true;
  }

  size_t
  RCLookupHandler::num_strict_connect_routers() const
  {
    return strict_connect_pubkeys.size();
  }

  bool
  RCLookupHandler::get_random_whitelist_router(RouterID& rid) const
  {
    return router->loop()->call_get([this, rid]() mutable {
      const auto sz = router_whitelist.size();
      auto itr = router_whitelist.begin();
      if (sz == 0)
        return false;
      if (sz > 1)
        std::advance(itr, randint() % sz);
      rid = *itr;
      return true;
    });
  }

  void
  RCLookupHandler::periodic_update(llarp_time_t now)
  {
    // try looking up stale routers
    std::unordered_set<RouterID> routersToLookUp;

    node_db->VisitInsertedBefore(
        [&](const RouterContact& rc) { routersToLookUp.insert(rc.router_id()); },
        now - RouterContact::REPUBLISH);

    for (const auto& router : routersToLookUp)
    {
      get_rc(router, nullptr, true);
    }

    node_db->remove_stale_rcs(boostrap_rid_list, now - RouterContact::STALE);
  }

  void
  RCLookupHandler::explore_network()
  {
    const size_t known = node_db->num_loaded();
    if (bootstrap_rc_list.empty() && known == 0)
    {
      LogError("we have no bootstrap nodes specified");
    }
    else if (known <= bootstrap_rc_list.size())
    {
      for (const auto& rc : bootstrap_rc_list)
      {
        const auto& rid = rc.router_id();
        log::info(link_cat, "Doing explore via bootstrap node: {}", rid);

        // TODO: replace this concept
        // dht->ExploreNetworkVia(dht::Key_t{rc.pubkey});
      }
    }

    if (isServiceNode)
    {
      static constexpr size_t LookupPerTick = 5;

      std::vector<RouterID> lookup_routers = router->loop()->call_get([this]() {
        std::vector<RouterID> lookups;
        lookups.reserve(LookupPerTick);

        for (const auto& r : router_whitelist)
        {
          if (not node_db->has_router(r))
            lookups.emplace_back(r);
        }

        return lookups;
      });

      if (lookup_routers.size() > LookupPerTick)
      {
        std::shuffle(lookup_routers.begin(), lookup_routers.end(), llarp::csrng);
        lookup_routers.resize(LookupPerTick);
      }

      for (const auto& r : lookup_routers)
        get_rc(r, nullptr, true);
      return;
    }
    // service nodes gossip, not explore
    if (contacts->router()->is_service_node())
      return;

    // explore via every connected peer
    /*
     * TODO: DHT explore via libquic
     *
    _linkManager->ForEachPeer([&](ILinkSession* s) {
      if (!s->IsEstablished())
        return;
      const RouterContact rc = s->GetRemoteRC();
      if (rc.IsPublicRouter() && (_bootstrapRCList.find(rc) == _bootstrapRCList.end()))
      {
        LogDebug("Doing explore via public node: ", RouterID(rc.pubkey));
        _dht->impl->ExploreNetworkVia(dht::Key_t{rc.pubkey});
      }
    });
     *
     *
     */
  }

  void
  RCLookupHandler::init(
      std::shared_ptr<Contacts> c,
      std::shared_ptr<NodeDB> nodedb,
      EventLoop_ptr l,
      std::function<void(std::function<void()>)> dowork,
      LinkManager* linkManager,
      service::Context* hiddenServiceContext,
      const std::unordered_set<RouterID>& strictConnectPubkeys,
      const std::set<RemoteRC>& bootstrapRCList,
      bool isServiceNode_arg)
  {
    contacts = c;
    node_db = std::move(nodedb);
    loop = std::move(l);
    work_func = std::move(dowork);
    hidden_service_context = hiddenServiceContext;
    strict_connect_pubkeys = strictConnectPubkeys;
    bootstrap_rc_list = bootstrapRCList;
    link_manager = linkManager;
    router = &link_manager->router();
    isServiceNode = isServiceNode_arg;

    for (const auto& rc : bootstrap_rc_list)
    {
      boostrap_rid_list.insert(rc.router_id());
    }
  }

  bool
  RCLookupHandler::is_remote_in_bootstrap(const RouterID& remote) const
  {
    for (const auto& rc : bootstrap_rc_list)
    {
      if (rc.router_id() == remote)
      {
        return true;
      }
    }
    return false;
  }

}  // namespace llarp
