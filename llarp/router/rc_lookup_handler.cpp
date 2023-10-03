#include <chrono>
#include "rc_lookup_handler.hpp"

#include <llarp/link/contacts.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/service/context.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/util/types.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/nodedb.hpp>
#include "router.hpp"

#include <iterator>
#include <functional>
#include <random>

namespace llarp
{
  void
  RCLookupHandler::add_valid_router(const RouterID& router)
  {
    util::Lock l(_mutex);
    router_whitelist.insert(router);
  }

  void
  RCLookupHandler::remove_valid_router(const RouterID& router)
  {
    util::Lock l(_mutex);
    router_whitelist.erase(router);
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
    util::Lock l(_mutex);

    loadColourList(router_whitelist, whitelist);
    loadColourList(router_greylist, greylist);
    loadColourList(router_greenlist, greenlist);

    LogInfo("lokinet service node list now has ", router_whitelist.size(), " active routers");
  }

  bool
  RCLookupHandler::has_received_whitelist() const
  {
    util::Lock l(_mutex);
    return not router_whitelist.empty();
  }

  void
  RCLookupHandler::get_rc(const RouterID& router, RCRequestCallback callback, bool forceLookup)
  {
    RouterContact remoteRC;
    if (not forceLookup)
    {
      if (const auto maybe = node_db->Get(router); maybe.has_value())
      {
        remoteRC = *maybe;
        if (callback)
        {
          callback(router, &remoteRC, RCRequestResult::Success);
        }
        finalize_request(router, &remoteRC, RCRequestResult::Success);
        return;
      }
    }
    bool shouldDoLookup = false;

    {
      util::Lock l(_mutex);

      auto itr_pair = pending_callbacks.emplace(router, callback_que{});

      if (callback)
      {
        itr_pair.first->second.push_back(callback);
      }
      shouldDoLookup = itr_pair.second;
    }

    if (shouldDoLookup)
    {
      auto fn = [this, router](const auto& res) { handle_dht_lookup_result(router, res); };

      // if we are a client try using the hidden service endpoints
      if (!isServiceNode)
      {
        bool sent = false;
        LogInfo("Lookup ", router, " anonymously");
        hidden_service_context->ForEachService(
            [&](const std::string&, const std::shared_ptr<service::Endpoint>& ep) -> bool {
              const bool success = ep->LookupRouterAnon(router, fn);
              sent = sent || success;
              return !success;
            });
        if (sent)
          return;
        LogWarn("cannot lookup ", router, " anonymously");
      }

      if (not contacts->lookup_router(router))
      {
        finalize_request(router, nullptr, RCRequestResult::RouterNotFound);
      }
      else
      {
        router_lookup_times[router] = std::chrono::steady_clock::now();
      }
    }
  }

  bool
  RCLookupHandler::is_grey_listed(const RouterID& remote) const
  {
    if (strict_connect_pubkeys.size() && strict_connect_pubkeys.count(remote) == 0
        && !is_remote_in_bootstrap(remote))
    {
      return false;
    }

    if (not useWhitelist)
      return false;

    util::Lock lock{_mutex};

    return router_greylist.count(remote);
  }

  bool
  RCLookupHandler::is_green_listed(const RouterID& remote) const
  {
    util::Lock lock{_mutex};
    return router_greenlist.count(remote);
  }

  bool
  RCLookupHandler::is_registered(const RouterID& remote) const
  {
    util::Lock lock{_mutex};
    return router_whitelist.count(remote) || router_greylist.count(remote)
        || router_greenlist.count(remote);
  }

  bool
  RCLookupHandler::is_path_allowed(const RouterID& remote) const
  {
    if (strict_connect_pubkeys.size() && strict_connect_pubkeys.count(remote) == 0
        && !is_remote_in_bootstrap(remote))
    {
      return false;
    }

    if (not useWhitelist)
      return true;

    util::Lock lock{_mutex};

    return router_whitelist.count(remote);
  }

  bool
  RCLookupHandler::is_session_allowed(const RouterID& remote) const
  {
    if (strict_connect_pubkeys.size() && strict_connect_pubkeys.count(remote) == 0
        && !is_remote_in_bootstrap(remote))
    {
      return false;
    }

    if (not useWhitelist)
      return true;

    util::Lock lock{_mutex};

    return router_whitelist.count(remote) or router_greylist.count(remote);
  }

  bool
  RCLookupHandler::check_rc(const RouterContact& rc) const
  {
    if (not is_session_allowed(rc.pubkey))
    {
      contacts->delete_rc_node_async(dht::Key_t{rc.pubkey});
      return false;
    }

    if (not rc.Verify(llarp::time_now_ms()))
    {
      LogWarn("RC for ", RouterID(rc.pubkey), " is invalid");
      return false;
    }

    // update nodedb if required
    if (rc.IsPublicRouter())
    {
      LogDebug("Adding or updating RC for ", RouterID(rc.pubkey), " to nodedb and dht.");
      loop->call([rc, n = node_db] { n->PutIfNewer(rc); });
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
  RCLookupHandler::get_random_whitelist_router(RouterID& router) const
  {
    util::Lock l(_mutex);

    const auto sz = router_whitelist.size();
    auto itr = router_whitelist.begin();
    if (sz == 0)
      return false;
    if (sz > 1)
      std::advance(itr, randint() % sz);
    router = *itr;
    return true;
  }

  bool
  RCLookupHandler::check_renegotiate_valid(RouterContact newrc, RouterContact oldrc)
  {
    // mismatch of identity ?
    if (newrc.pubkey != oldrc.pubkey)
      return false;

    if (!is_session_allowed(newrc.pubkey))
      return false;

    auto func = [this, newrc] { check_rc(newrc); };
    work_func(func);

    // update dht if required
    if (contacts->rc_nodes()->HasNode(dht::Key_t{newrc.pubkey}))
    {
      contacts->rc_nodes()->PutNode(newrc);
    }

    // TODO: check for other places that need updating the RC
    return true;
  }

  void
  RCLookupHandler::periodic_update(llarp_time_t now)
  {
    // try looking up stale routers
    std::unordered_set<RouterID> routersToLookUp;

    node_db->VisitInsertedBefore(
        [&](const RouterContact& rc) {
          if (has_pending_lookup(rc.pubkey))
            return;
          routersToLookUp.insert(rc.pubkey);
        },
        now - RouterContact::UpdateInterval);

    for (const auto& router : routersToLookUp)
    {
      get_rc(router, nullptr, true);
    }

    node_db->RemoveStaleRCs(boostrap_rid_list, now - RouterContact::StaleInsertionAge);
  }

  void
  RCLookupHandler::explore_network()
  {
    const size_t known = node_db->NumLoaded();
    if (bootstrap_rc_list.empty() && known == 0)
    {
      LogError("we have no bootstrap nodes specified");
    }
    else if (known <= bootstrap_rc_list.size())
    {
      for (const auto& rc : bootstrap_rc_list)
      {
        LogInfo("Doing explore via bootstrap node: ", RouterID(rc.pubkey));
        // TODO: replace this concept
        // dht->ExploreNetworkVia(dht::Key_t{rc.pubkey});
      }
    }

    if (useWhitelist)
    {
      static constexpr auto RerequestInterval = 10min;
      static constexpr size_t LookupPerTick = 5;

      std::vector<RouterID> lookupRouters;
      lookupRouters.reserve(LookupPerTick);

      const auto now = std::chrono::steady_clock::now();

      {
        // if we are using a whitelist look up a few routers we don't have
        util::Lock l(_mutex);
        for (const auto& r : router_whitelist)
        {
          if (now > router_lookup_times[r] + RerequestInterval and not node_db->Has(r))
          {
            lookupRouters.emplace_back(r);
          }
        }
      }

      if (lookupRouters.size() > LookupPerTick)
      {
        std::shuffle(lookupRouters.begin(), lookupRouters.end(), CSRNG{});
        lookupRouters.resize(LookupPerTick);
      }

      for (const auto& r : lookupRouters)
        get_rc(r, nullptr, true);
      return;
    }
    // service nodes gossip, not explore
    if (contacts->router()->IsServiceNode())
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
      worker_func dowork,
      LinkManager* linkManager,
      service::Context* hiddenServiceContext,
      const std::unordered_set<RouterID>& strictConnectPubkeys,
      const std::set<RouterContact>& bootstrapRCList,
      bool useWhitelist_arg,
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
    useWhitelist = useWhitelist_arg;
    isServiceNode = isServiceNode_arg;

    for (const auto& rc : bootstrap_rc_list)
    {
      boostrap_rid_list.insert(rc.pubkey);
    }
  }

  void
  RCLookupHandler::handle_dht_lookup_result(
      RouterID remote, const std::vector<RouterContact>& results)
  {
    if (not results.size())
    {
      finalize_request(remote, nullptr, RCRequestResult::RouterNotFound);
      return;
    }

    if (not is_session_allowed(remote))
    {
      finalize_request(remote, &results[0], RCRequestResult::InvalidRouter);
      return;
    }

    if (not check_rc(results[0]))
    {
      finalize_request(remote, &results[0], RCRequestResult::BadRC);
      return;
    }

    finalize_request(remote, &results[0], RCRequestResult::Success);
  }

  bool
  RCLookupHandler::has_pending_lookup(RouterID remote) const
  {
    util::Lock l(_mutex);
    return pending_callbacks.find(remote) != pending_callbacks.end();
  }

  bool
  RCLookupHandler::is_remote_in_bootstrap(const RouterID& remote) const
  {
    for (const auto& rc : bootstrap_rc_list)
    {
      if (rc.pubkey == remote)
      {
        return true;
      }
    }
    return false;
  }

  void
  RCLookupHandler::finalize_request(
      const RouterID& router, const RouterContact* const rc, RCRequestResult result)
  {
    callback_que movedCallbacks;
    {
      util::Lock l(_mutex);

      auto itr = pending_callbacks.find(router);

      if (itr != pending_callbacks.end())
      {
        movedCallbacks.splice(movedCallbacks.begin(), itr->second);
        pending_callbacks.erase(itr);
      }
    }  // lock

    for (const auto& callback : movedCallbacks)
    {
      callback(router, rc, result);
    }
  }

}  // namespace llarp
