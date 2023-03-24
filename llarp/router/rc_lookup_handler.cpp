#include <chrono>
#include "rc_lookup_handler.hpp"

#include <llarp/link/i_link_manager.hpp>
#include <llarp/link/server.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/util/types.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/dht/context.hpp>
#include "abstractrouter.hpp"

#include <iterator>
#include <functional>
#include <random>

namespace llarp
{
  void
  RCLookupHandler::AddValidRouter(const RouterID& router)
  {
    util::Lock l(_mutex);
    whitelistRouters.insert(router);
  }

  void
  RCLookupHandler::RemoveValidRouter(const RouterID& router)
  {
    util::Lock l(_mutex);
    whitelistRouters.erase(router);
  }

  static void
  loadColourList(std::unordered_set<RouterID>& beigelist, const std::vector<RouterID>& new_beige)
  {
    beigelist.clear();
    beigelist.insert(new_beige.begin(), new_beige.end());
  }

  RCLookupHandler::RCLookupHandler(AbstractRouter& router) : _router{router}
  {}

  void
  RCLookupHandler::SetRouterWhitelist(
      const std::vector<RouterID>& whitelist,
      const std::vector<RouterID>& greylist,
      const std::vector<RouterID>& greenlist)
  {
    if (whitelist.empty())
      return;
    util::Lock l(_mutex);

    loadColourList(whitelistRouters, whitelist);
    loadColourList(greylistRouters, greylist);
    loadColourList(greenlistRouters, greenlist);

    LogInfo("lokinet service node list now has ", whitelistRouters.size(), " active routers");
  }

  bool
  RCLookupHandler::HaveReceivedWhitelist() const
  {
    util::Lock l(_mutex);
    return not whitelistRouters.empty();
  }

  void
  RCLookupHandler::GetRC(const RouterID& router, RCRequestCallback callback, bool forceLookup)
  {
    RouterContact remoteRC;
    if (not forceLookup)
    {
      if (auto maybe = _router.nodedb()->Get(router))
      {
        remoteRC = std::move(*maybe);
        if (callback)
        {
          callback(router, &remoteRC, RCRequestResult::Success);
        }
        FinalizeRequest(router, &remoteRC, RCRequestResult::Success);
        return;
      }
    }
    bool shouldDoLookup = false;

    {
      util::Lock l(_mutex);

      auto itr_pair = pendingCallbacks.emplace(router, CallbacksQueue{});

      if (callback)
      {
        itr_pair.first->second.push_back(callback);
      }
      shouldDoLookup = itr_pair.second;
    }

    if (shouldDoLookup)
    {
      auto fn = [this, router](const auto& res) { HandleDHTLookupResult(router, res); };
      auto lookup = [this](RouterID target, RouterLookupHandler handler) -> bool {
        if (_router.IsServiceNode())
          return _router.dht()->impl->LookupRouter(target, std::move(handler));
        // todo: client lookup via path.
        return false;
      };

      if (not lookup(router, std::move(fn)))
      {
        FinalizeRequest(router, nullptr, RCRequestResult::RouterNotFound);
      }
      else
      {
        _routerLookupTimes[router] = std::chrono::steady_clock::now();
      }
    }
  }

  bool
  RCLookupHandler::IsGreylisted(const RouterID& remote) const
  {
    if (_strictConnectPubkeys.size() && _strictConnectPubkeys.count(remote) == 0
        && !RemoteInBootstrap(remote))
    {
      return false;
    }

    if (not useWhitelist)
      return false;

    util::Lock lock{_mutex};

    return greylistRouters.count(remote);
  }

  bool
  RCLookupHandler::IsGreenlisted(const RouterID& remote) const
  {
    util::Lock lock{_mutex};
    return greenlistRouters.count(remote);
  }

  bool
  RCLookupHandler::IsRegistered(const RouterID& remote) const
  {
    util::Lock lock{_mutex};
    return whitelistRouters.count(remote) || greylistRouters.count(remote)
        || greenlistRouters.count(remote);
  }

  bool
  RCLookupHandler::PathIsAllowed(const RouterID& remote) const
  {
    if (_strictConnectPubkeys.count(remote) == 1 and not isServiceNode)
      return true;
    // clients shall not build paths over bootstrap node.
    if (RemoteInBootstrap(remote) and not isServiceNode)
      return false;

    if (not useWhitelist)
      return true;

    util::Lock lock{_mutex};

    return whitelistRouters.count(remote);
  }

  bool
  RCLookupHandler::SessionIsAllowed(const RouterID& remote) const
  {
    if (_strictConnectPubkeys.count(remote) == 1 and not isServiceNode)
      return true;
    if (RemoteInBootstrap(remote))
      return true;

    if (not useWhitelist)
      return true;

    util::Lock lock{_mutex};

    return whitelistRouters.count(remote) or greylistRouters.count(remote);
  }

  bool
  RCLookupHandler::CheckRC(const RouterContact& rc) const
  {
    if (not SessionIsAllowed(rc.pubkey))
    {
      _router.dht()->impl->DelRCNodeAsync(dht::Key_t{rc.pubkey});
      return false;
    }

    if (not rc.Verify(_router.dht()->impl->Now()))
    {
      LogWarn("RC for ", RouterID(rc.pubkey), " is invalid");
      return false;
    }

    // update nodedb if required
    if (rc.IsPublicRouter())
    {
      LogDebug("Adding or updating RC for ", RouterID(rc.pubkey), " to nodedb and dht.");
      _router.loop()->call([rc, n = _router.nodedb()] { n->PutIfNewer(rc); });
      _router.dht()->impl->PutRCNodeAsync(rc);
    }

    return true;
  }

  size_t
  RCLookupHandler::NumberOfStrictConnectRouters() const
  {
    return _strictConnectPubkeys.size();
  }

  bool
  RCLookupHandler::GetRandomWhitelistRouter(RouterID& router) const
  {
    util::Lock l(_mutex);

    const auto sz = whitelistRouters.size();
    auto itr = whitelistRouters.begin();
    if (sz == 0)
      return false;
    if (sz > 1)
      std::advance(itr, randint() % sz);
    router = *itr;
    return true;
  }

  bool
  RCLookupHandler::CheckRenegotiateValid(RouterContact newrc, RouterContact oldrc)
  {
    // mismatch of identity ?
    if (newrc.pubkey != oldrc.pubkey)
      return false;

    if (!SessionIsAllowed(newrc.pubkey))
      return false;

    _router.QueueWork([this, newrc] { CheckRC(newrc); });

    // update dht if required
    if (_router.dht()->impl->Nodes()->HasNode(dht::Key_t{newrc.pubkey}))
    {
      _router.dht()->impl->Nodes()->PutNode(newrc);
    }

    // TODO: check for other places that need updating the RC
    return true;
  }

  void
  RCLookupHandler::PeriodicUpdate(llarp_time_t now)
  {
    // try looking up stale routers
    std::unordered_set<RouterID> routersToLookUp;

    _router.nodedb()->VisitInsertedBefore(
        [this, &routersToLookUp](const RouterContact& rc) {
          if (HavePendingLookup(rc.pubkey))
            return;
          routersToLookUp.insert(rc.pubkey);
        },
        now - RouterContact::UpdateInterval);

    for (const auto& router : routersToLookUp)
    {
      GetRC(router, nullptr, true);
    }

    _router.nodedb()->RemoveStaleRCs(
        _bootstrapRouterIDList, now - RouterContact::StaleInsertionAge);
  }

  void
  RCLookupHandler::ExploreNetwork()
  {
    const size_t known = _router.nodedb()->NumLoaded();
    if (_bootstrapRCList.empty() && known == 0)
    {
      LogError("we have no bootstrap nodes specified");
    }
    else if (known <= _bootstrapRCList.size())
    {
      for (const auto& rc : _bootstrapRCList)
      {
        LogInfo("Doing explore via bootstrap node: ", RouterID(rc.pubkey));
        _router.dht()->impl->ExploreNetworkVia(dht::Key_t{rc.pubkey});
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
        for (const auto& r : whitelistRouters)
        {
          if (now > _routerLookupTimes[r] + RerequestInterval and not _router.nodedb()->Has(r))
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
        GetRC(r, nullptr, true);
      return;
    }
    // service nodes gossip, not explore
    if (_router.dht()->impl->GetRouter()->IsServiceNode())
      return;

    // explore via every connected peer
    _router.linkManager().ForEachPeer([this](ILinkSession* s) {
      if (!s->IsEstablished())
        return;
      const RouterContact rc = s->GetRemoteRC();
      if (rc.IsPublicRouter() && (_bootstrapRCList.find(rc) == _bootstrapRCList.end()))
      {
        LogDebug("Doing explore via public node: ", RouterID(rc.pubkey));
        _router.dht()->impl->ExploreNetworkVia(dht::Key_t{rc.pubkey});
      }
    });
  }

  void
  RCLookupHandler::Init(
      const std::unordered_set<RouterID>& strictConnectPubkeys,
      const std::set<RouterContact>& bootstrapRCList,
      bool useWhitelist_arg,
      bool isServiceNode_arg)
  {
    _strictConnectPubkeys = strictConnectPubkeys;
    _bootstrapRCList = bootstrapRCList;

    useWhitelist = useWhitelist_arg;
    isServiceNode = isServiceNode_arg;

    for (const auto& rc : _bootstrapRCList)
    {
      _bootstrapRouterIDList.insert(rc.pubkey);
    }
  }

  void
  RCLookupHandler::HandleDHTLookupResult(RouterID remote, const std::vector<RouterContact>& results)
  {
    if (not results.size())
    {
      FinalizeRequest(remote, nullptr, RCRequestResult::RouterNotFound);
      return;
    }

    if (not SessionIsAllowed(remote))
    {
      FinalizeRequest(remote, &results[0], RCRequestResult::InvalidRouter);
      return;
    }

    if (not CheckRC(results[0]))
    {
      FinalizeRequest(remote, &results[0], RCRequestResult::BadRC);
      return;
    }

    FinalizeRequest(remote, &results[0], RCRequestResult::Success);
  }

  bool
  RCLookupHandler::HavePendingLookup(RouterID remote) const
  {
    util::Lock l(_mutex);
    return pendingCallbacks.find(remote) != pendingCallbacks.end();
  }

  bool
  RCLookupHandler::RemoteInBootstrap(const RouterID& remote) const
  {
    for (const auto& rc : _bootstrapRCList)
    {
      if (rc.pubkey == remote)
      {
        return true;
      }
    }
    return false;
  }

  void
  RCLookupHandler::FinalizeRequest(
      const RouterID& router, const RouterContact* const rc, RCRequestResult result)
  {
    CallbacksQueue movedCallbacks;
    {
      util::Lock l(_mutex);

      auto itr = pendingCallbacks.find(router);

      if (itr != pendingCallbacks.end())
      {
        movedCallbacks.splice(movedCallbacks.begin(), itr->second);
        pendingCallbacks.erase(itr);
      }
    }  // lock

    for (const auto& callback : movedCallbacks)
    {
      callback(router, rc, result);
    }
  }

}  // namespace llarp
