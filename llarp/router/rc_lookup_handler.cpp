#include <chrono>
#include <router/rc_lookup_handler.hpp>

#include <link/i_link_manager.hpp>
#include <link/server.hpp>
#include <crypto/crypto.hpp>
#include <service/context.hpp>
#include <router_contact.hpp>
#include <util/meta/memfn.hpp>
#include <util/types.hpp>
#include <util/thread/threading.hpp>
#include <nodedb.hpp>
#include <dht/context.hpp>
#include <router/abstractrouter.hpp>

#include <iterator>
#include <functional>
#include <random>

using namespace std::chrono_literals;

namespace llarp
{
  void
  RCLookupHandler::AddValidRouter(const RouterID &router)
  {
    util::Lock l(&_mutex);
    whitelistRouters.insert(router);
  }

  void
  RCLookupHandler::RemoveValidRouter(const RouterID &router)
  {
    util::Lock l(&_mutex);
    whitelistRouters.erase(router);
  }

  void
  RCLookupHandler::SetRouterWhitelist(const std::vector< RouterID > &routers)
  {
    if(routers.empty())
      return;
    util::Lock l(&_mutex);

    whitelistRouters.clear();
    for(auto &router : routers)
    {
      whitelistRouters.emplace(router);
    }

    LogInfo("lokinet service node list now has ", whitelistRouters.size(),
            " routers");
  }

  bool
  RCLookupHandler::HaveReceivedWhitelist()
  {
    util::Lock l(&_mutex);
    return not whitelistRouters.empty();
  }

  void
  RCLookupHandler::GetRC(const RouterID &router, RCRequestCallback callback,
                         bool forceLookup)
  {
    RouterContact remoteRC;
    if(not forceLookup)
    {
      if(_nodedb->Get(router, remoteRC))
      {
        if(callback)
        {
          callback(router, &remoteRC, RCRequestResult::Success);
        }
        FinalizeRequest(router, &remoteRC, RCRequestResult::Success);
        return;
      }
    }
    bool shouldDoLookup = false;

    {
      util::Lock l(&_mutex);

      auto itr_pair = pendingCallbacks.emplace(router, CallbacksQueue{});

      if(callback)
      {
        itr_pair.first->second.push_back(callback);
      }
      shouldDoLookup = itr_pair.second;
    }

    if(shouldDoLookup)
    {
      auto fn = std::bind(&RCLookupHandler::HandleDHTLookupResult, this, router,
                          std::placeholders::_1);

      // if we are a client try using the hidden service endpoints
      if(!isServiceNode)
      {
        bool sent = false;
        LogInfo("Lookup ", router, " anonymously");
        _hiddenServiceContext->ForEachService(
            [&](const std::string &,
                const std::shared_ptr< service::Endpoint > &ep) -> bool {
              const bool success = ep->LookupRouterAnon(router, fn);
              sent               = sent || success;
              return !success;
            });
        if(sent)
          return;
        LogWarn("cannot lookup ", router, " anonymously");
      }

      if(!_dht->impl->LookupRouter(router, fn))
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
  RCLookupHandler::RemoteIsAllowed(const RouterID &remote) const
  {
    if(_strictConnectPubkeys.size() && _strictConnectPubkeys.count(remote) == 0
       && !RemoteInBootstrap(remote))
    {
      return false;
    }

    util::Lock l(&_mutex);

    if(useWhitelist && whitelistRouters.find(remote) == whitelistRouters.end())
    {
      return false;
    }

    return true;
  }

  bool
  RCLookupHandler::CheckRC(const RouterContact &rc) const
  {
    if(not RemoteIsAllowed(rc.pubkey))
    {
      _dht->impl->DelRCNodeAsync(dht::Key_t{rc.pubkey});
      return false;
    }

    if(not rc.Verify(_dht->impl->Now()))
    {
      LogWarn("RC for ", RouterID(rc.pubkey), " is invalid");
      return false;
    }

    // update nodedb if required
    if(rc.IsPublicRouter())
    {
      LogDebug("Adding or updating RC for ", RouterID(rc.pubkey),
               " to nodedb and dht.");
      _nodedb->UpdateAsyncIfNewer(rc);
      _dht->impl->PutRCNodeAsync(rc);
    }

    return true;
  }

  size_t
  RCLookupHandler::NumberOfStrictConnectRouters() const
  {
    return _strictConnectPubkeys.size();
  }

  bool
  RCLookupHandler::GetRandomWhitelistRouter(RouterID &router) const
  {
    util::Lock l(&_mutex);

    const auto sz = whitelistRouters.size();
    auto itr      = whitelistRouters.begin();
    if(sz == 0)
      return false;
    if(sz > 1)
      std::advance(itr, randint() % sz);
    router = *itr;
    return true;
  }

  bool
  RCLookupHandler::CheckRenegotiateValid(RouterContact newrc,
                                         RouterContact oldrc)
  {
    // missmatch of identity ?
    if(newrc.pubkey != oldrc.pubkey)
      return false;

    if(!RemoteIsAllowed(newrc.pubkey))
      return false;

    auto func = std::bind(&RCLookupHandler::CheckRC, this, newrc);
    _threadpool->addJob(func);

    // update dht if required
    if(_dht->impl->Nodes()->HasNode(dht::Key_t{newrc.pubkey}))
    {
      _dht->impl->Nodes()->PutNode(newrc);
    }

    // TODO: check for other places that need updating the RC
    return true;
  }

  void
  RCLookupHandler::PeriodicUpdate(llarp_time_t now)
  {
    // try looking up stale routers
    std::set< RouterID > routersToLookUp;

    _nodedb->VisitInsertedBefore(
        [&](const RouterContact &rc) {
          if(HavePendingLookup(rc.pubkey))
            return;
          routersToLookUp.insert(rc.pubkey);
        },
        now - RouterContact::UpdateInterval);

    for(const auto &router : routersToLookUp)
    {
      GetRC(router, nullptr, true);
    }

    _nodedb->RemoveStaleRCs(_bootstrapRouterIDList,
                            now - RouterContact::StaleInsertionAge);
  }

  void
  RCLookupHandler::ExploreNetwork()
  {
    if(_bootstrapRCList.size())
    {
      for(const auto &rc : _bootstrapRCList)
      {
        LogInfo("Doing explore via bootstrap node: ", RouterID(rc.pubkey));
        _dht->impl->ExploreNetworkVia(dht::Key_t{rc.pubkey});
      }
    }
    else
    {
      LogError("we have no bootstrap nodes specified");
    }

    if(useWhitelist)
    {
      static constexpr size_t LookupPerTick   = 25;
      static constexpr auto RerequestInterval = 10min;

      std::vector< RouterID > lookupRouters;
      lookupRouters.reserve(LookupPerTick);

      const auto now = std::chrono::steady_clock::now();

      {
        // if we are using a whitelist look up a few routers we don't have
        util::Lock l(&_mutex);
        for(const auto &r : whitelistRouters)
        {
          if(now > _routerLookupTimes[r] + RerequestInterval
             and not _nodedb->Has(r))
          {
            lookupRouters.emplace_back(r);
          }
        }
      }

      if(lookupRouters.size() > LookupPerTick)
      {
        static std::mt19937_64 rng{std::random_device{}()};
        std::shuffle(lookupRouters.begin(), lookupRouters.end(), rng);
        lookupRouters.resize(LookupPerTick);
      }

      for(const auto &r : lookupRouters)
        GetRC(r, nullptr, true);
      return;
    }
    // TODO: only explore via random subset
    // explore via every connected peer
    _linkManager->ForEachPeer([&](ILinkSession *s) {
      if(!s->IsEstablished())
        return;
      const RouterContact rc = s->GetRemoteRC();
      if(rc.IsPublicRouter()
         && (_bootstrapRCList.find(rc) == _bootstrapRCList.end()))
      {
        LogInfo("Doing explore via public node: ", RouterID(rc.pubkey));
        _dht->impl->ExploreNetworkVia(dht::Key_t{rc.pubkey});
      }
    });
  }

  void
  RCLookupHandler::Init(llarp_dht_context *dht, llarp_nodedb *nodedb,
                        std::shared_ptr< llarp::thread::ThreadPool > threadpool,
                        ILinkManager *linkManager,
                        service::Context *hiddenServiceContext,
                        const std::set< RouterID > &strictConnectPubkeys,
                        const std::set< RouterContact > &bootstrapRCList,
                        bool useWhitelist_arg, bool isServiceNode_arg)
  {
    _dht                  = dht;
    _nodedb               = nodedb;
    _threadpool           = threadpool;
    _hiddenServiceContext = hiddenServiceContext;
    _strictConnectPubkeys = strictConnectPubkeys;
    _bootstrapRCList      = bootstrapRCList;
    _linkManager          = linkManager;
    useWhitelist          = useWhitelist_arg;
    isServiceNode         = isServiceNode_arg;

    for(const auto &rc : _bootstrapRCList)
    {
      _bootstrapRouterIDList.insert(rc.pubkey);
    }
  }

  void
  RCLookupHandler::HandleDHTLookupResult(
      RouterID remote, const std::vector< RouterContact > &results)
  {
    if(not results.size())
    {
      FinalizeRequest(remote, nullptr, RCRequestResult::RouterNotFound);
      return;
    }

    if(not RemoteIsAllowed(remote))
    {
      FinalizeRequest(remote, &results[0], RCRequestResult::InvalidRouter);
      return;
    }

    if(not CheckRC(results[0]))
    {
      FinalizeRequest(remote, &results[0], RCRequestResult::BadRC);
      return;
    }

    FinalizeRequest(remote, &results[0], RCRequestResult::Success);
  }

  bool
  RCLookupHandler::HavePendingLookup(RouterID remote) const
  {
    util::Lock l(&_mutex);
    return pendingCallbacks.find(remote) != pendingCallbacks.end();
  }

  bool
  RCLookupHandler::RemoteInBootstrap(const RouterID &remote) const
  {
    for(const auto &rc : _bootstrapRCList)
    {
      if(rc.pubkey == remote)
      {
        return true;
      }
    }
    return false;
  }

  void
  RCLookupHandler::FinalizeRequest(const RouterID &router,
                                   const RouterContact *const rc,
                                   RCRequestResult result)
  {
    CallbacksQueue movedCallbacks;
    {
      util::Lock l(&_mutex);

      auto itr = pendingCallbacks.find(router);

      if(itr != pendingCallbacks.end())
      {
        movedCallbacks.splice(movedCallbacks.begin(), itr->second);
        pendingCallbacks.erase(itr);
      }
    }  // lock

    for(const auto &callback : movedCallbacks)
    {
      callback(router, rc, result);
    }
  }

}  // namespace llarp
