#include "context.hpp"

#include "explorenetworkjob.hpp"
#include "localrouterlookup.hpp"
#include "localserviceaddresslookup.hpp"
#include "localtaglookup.hpp"
#include <llarp/dht/messages/findrouter.hpp>
#include <llarp/dht/messages/gotintro.hpp>
#include <llarp/dht/messages/gotrouter.hpp>
#include <llarp/dht/messages/pubintro.hpp>
#include "node.hpp"
#include "publishservicejob.hpp"
#include "recursiverouterlookup.hpp"
#include "serviceaddresslookup.hpp"
#include "taglookup.hpp"
#include <llarp/messages/dht_immediate.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/dht_message.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router/i_rc_lookup_handler.hpp>
#include <llarp/util/decaying_hashset.hpp>
#include <vector>

namespace llarp
{
  namespace dht
  {
    AbstractContext::~AbstractContext() = default;

    struct Context final : public AbstractContext
    {
      Context();

      ~Context() override = default;

      util::StatusObject
      ExtractStatus() const override;

      void
      StoreRC(const RouterContact rc) const override
      {
        GetRouter()->rcLookupHandler().CheckRC(rc);
      }

      void
      LookupIntroSetRelayed(
          const Key_t& target,
          const Key_t& whoasked,
          uint64_t whoaskedTX,
          const Key_t& askpeer,
          uint64_t relayOrder,
          service::EncryptedIntroSetLookupHandler result = nullptr) override;

      void
      LookupIntroSetDirect(
          const Key_t& target,
          const Key_t& whoasked,
          uint64_t whoaskedTX,
          const Key_t& askpeer,
          service::EncryptedIntroSetLookupHandler result = nullptr) override;

      /// on behalf of whoasked request router with public key target from dht
      /// router with key askpeer
      void
      LookupRouterRecursive(
          const RouterID& target,
          const Key_t& whoasked,
          uint64_t whoaskedTX,
          const Key_t& askpeer,
          RouterLookupHandler result = nullptr) override;

      bool
      LookupRouter(const RouterID& target, RouterLookupHandler result) override
      {
        Key_t askpeer;
        if (!_nodes->FindClosest(Key_t(target), askpeer))
        {
          return false;
        }
        LookupRouterRecursive(target, OurKey(), 0, askpeer, result);
        return true;
      }

      bool
      HasRouterLookup(const RouterID& target) const override
      {
        return pendingRouterLookups().HasLookupFor(target);
      }

      /// issue dht lookup for router via askpeer and send reply to local path
      void
      LookupRouterForPath(
          const RouterID& target,
          uint64_t txid,
          const PathID_t& path,
          const Key_t& askpeer) override;

      /// issue dht lookup for introset for addr via askpeer and send reply to
      /// local path
      void
      LookupIntroSetForPath(
          const Key_t& addr,
          uint64_t txid,
          const llarp::PathID_t& path,
          const Key_t& askpeer,
          uint64_t relayOrder) override;

      /// send a dht message to peer, if keepalive is true then keep the session
      /// with that peer alive for 10 seconds
      void
      DHTSendTo(const RouterID& peer, IMessage* msg, bool keepalive = true) override;

      /// get routers closest to target excluding requester
      bool
      HandleExploritoryRouterLookup(
          const Key_t& requester,
          uint64_t txid,
          const RouterID& target,
          std::vector<std::unique_ptr<IMessage>>& reply) override;

      /// handle rc lookup from requester for target
      void
      LookupRouterRelayed(
          const Key_t& requester,
          uint64_t txid,
          const Key_t& target,
          bool recursive,
          std::vector<std::unique_ptr<IMessage>>& replies) override;

      /// relay a dht message from a local path to the main network
      bool
      RelayRequestForPath(const llarp::PathID_t& localPath, const IMessage& msg) override;

      /// send introset to peer as R/S
      void
      PropagateLocalIntroSet(
          const PathID_t& from,
          uint64_t txid,
          const service::EncryptedIntroSet& introset,
          const Key_t& tellpeer,
          uint64_t relayOrder) override;

      /// send introset to peer from source with S counter and excluding peers
      void
      PropagateIntroSetTo(
          const Key_t& from,
          uint64_t txid,
          const service::EncryptedIntroSet& introset,
          const Key_t& tellpeer,
          uint64_t relayOrder) override;

      /// initialize dht context and explore every exploreInterval milliseconds
      void
      Init(const Key_t& us, AbstractRouter* router) override;

      /// get localally stored introset by service address
      std::optional<llarp::service::EncryptedIntroSet>
      GetIntroSetByLocation(const Key_t& location) const override;

      void
      handle_cleaner_timer();

      /// explore dht for new routers
      void
      Explore(size_t N = 3);

      llarp::AbstractRouter* router{nullptr};
      // for router contacts
      std::unique_ptr<Bucket<RCNode>> _nodes;

      // for introduction sets
      std::unique_ptr<Bucket<ISNode>> _services;

      Bucket<ISNode>*
      services() override
      {
        return _services.get();
      }

      bool allowTransit{false};

      bool&
      AllowTransit() override
      {
        return allowTransit;
      }
      const bool&
      AllowTransit() const override
      {
        return allowTransit;
      }

      Bucket<RCNode>*
      Nodes() const override
      {
        return _nodes.get();
      }

      void
      PutRCNodeAsync(const RCNode& val) override
      {
        router->loop()->call([nodes = Nodes(), val] { nodes->PutNode(val); });
      }

      void
      DelRCNodeAsync(const Key_t& val) override
      {
        router->loop()->call([nodes = Nodes(), val] { nodes->DelNode(val); });
      }

      const Key_t&
      OurKey() const override
      {
        return ourKey;
      }

      llarp::AbstractRouter*
      GetRouter() const override
      {
        return router;
      }

      bool
      GetRCFromNodeDB(const Key_t& k, llarp::RouterContact& rc) const override
      {
        if (const auto maybe = router->nodedb()->Get(k.as_array()); maybe.has_value())
        {
          rc = *maybe;
          return true;
        }
        return false;
      }

      PendingIntrosetLookups _pendingIntrosetLookups;
      PendingRouterLookups _pendingRouterLookups;
      PendingExploreLookups _pendingExploreLookups;

      PendingIntrosetLookups&
      pendingIntrosetLookups() override
      {
        return _pendingIntrosetLookups;
      }

      const PendingIntrosetLookups&
      pendingIntrosetLookups() const override
      {
        return _pendingIntrosetLookups;
      }

      PendingRouterLookups&
      pendingRouterLookups() override
      {
        return _pendingRouterLookups;
      }

      const PendingRouterLookups&
      pendingRouterLookups() const override
      {
        return _pendingRouterLookups;
      }

      PendingExploreLookups&
      pendingExploreLookups() override
      {
        return _pendingExploreLookups;
      }

      const PendingExploreLookups&
      pendingExploreLookups() const override
      {
        return _pendingExploreLookups;
      }

      uint64_t
      NextID()
      {
        return ++ids;
      }

      llarp_time_t
      Now() const override;

      void
      ExploreNetworkVia(const Key_t& peer) override;

     private:
      std::shared_ptr<int> _timer_keepalive;

      void
      CleanupTX();

      uint64_t ids;

      Key_t ourKey;
    };

    Context::Context()
    {
      randombytes((byte_t*)&ids, sizeof(uint64_t));
    }

    void
    Context::Explore(size_t N)
    {
      // ask N random peers for new routers
      llarp::LogDebug("Exploring network via ", N, " peers");
      std::set<Key_t> peers;

      if (_nodes->GetManyRandom(peers, N))
      {
        for (const auto& peer : peers)
          ExploreNetworkVia(peer);
      }
      else
        llarp::LogError("failed to select ", N, " random nodes for exploration");
    }

    void
    Context::ExploreNetworkVia(const Key_t& askpeer)
    {
      uint64_t txid = ++ids;
      const TXOwner peer(askpeer, txid);
      const TXOwner whoasked(OurKey(), txid);
      const RouterID K(askpeer.as_array());
      pendingExploreLookups().NewTX(
          peer, whoasked, K, new ExploreNetworkJob(askpeer.as_array(), this));
    }

    void
    Context::handle_cleaner_timer()
    {
      // clean up transactions
      CleanupTX();
      const llarp_time_t now = Now();

      if (_nodes)
      {
        // expire router contacts in memory
        auto& nodes = _nodes->nodes;
        auto itr = nodes.begin();
        while (itr != nodes.end())
        {
          if (itr->second.rc.IsExpired(now))
          {
            itr = nodes.erase(itr);
          }
          else
            ++itr;
        }
      }

      if (_services)
      {
        // expire intro sets
        auto& nodes = _services->nodes;
        auto itr = nodes.begin();
        while (itr != nodes.end())
        {
          if (itr->second.introset.IsExpired(now))
          {
            itr = nodes.erase(itr);
          }
          else
            ++itr;
        }
      }
    }

    void
    Context::LookupRouterRelayed(
        const Key_t& requester,
        uint64_t txid,
        const Key_t& target,
        bool recursive,
        std::vector<std::unique_ptr<IMessage>>& replies)
    {
      if (target == ourKey)
      {
        // we are the target, give them our RC
        replies.emplace_back(new GotRouterMessage(requester, txid, {router->rc()}, false));
        return;
      }
      if (not GetRouter()->SessionToRouterAllowed(target.as_array()))
      {
        // explicitly not allowed
        replies.emplace_back(new GotRouterMessage(requester, txid, {}, false));
        return;
      }
      const auto rc = GetRouter()->nodedb()->FindClosestTo(target);
      const Key_t next(rc.pubkey);
      {
        if (next == target)
        {
          // we know the target
          if (rc.ExpiresSoon(llarp::time_now_ms()))
          {
            // ask target for their rc to keep it updated
            LookupRouterRecursive(target.as_array(), requester, txid, next);
          }
          else
          {
            // send reply with rc we know of
            replies.emplace_back(new GotRouterMessage(requester, txid, {rc}, false));
          }
        }
        else if (recursive)  // are we doing a recursive lookup?
        {
          // is the next peer we ask closer to the target than us?
          if ((next ^ target) < (ourKey ^ target))
          {
            // yes it is closer, ask neighbour recursively
            LookupRouterRecursive(target.as_array(), requester, txid, next);
          }
          else
          {
            // no we are closer to the target so tell requester it's not there
            // so they switch to iterative lookup
            replies.emplace_back(new GotRouterMessage(requester, txid, {}, false));
          }
        }
        else
        {
          // iterative lookup and we don't have it tell them who is closer
          replies.emplace_back(new GotRouterMessage(requester, next, txid, false));
        }
      }
    }

    std::optional<llarp::service::EncryptedIntroSet>
    Context::GetIntroSetByLocation(const Key_t& key) const
    {
      auto itr = _services->nodes.find(key);
      if (itr == _services->nodes.end())
        return {};
      return itr->second.introset;
    }

    void
    Context::CleanupTX()
    {
      auto now = Now();
      llarp::LogDebug("DHT tick");

      pendingRouterLookups().Expire(now);
      _pendingIntrosetLookups.Expire(now);
      pendingExploreLookups().Expire(now);
    }

    util::StatusObject
    Context::ExtractStatus() const
    {
      util::StatusObject obj{
          {"pendingRouterLookups", pendingRouterLookups().ExtractStatus()},
          {"pendingIntrosetLookups", _pendingIntrosetLookups.ExtractStatus()},
          {"pendingExploreLookups", pendingExploreLookups().ExtractStatus()},
          {"nodes", _nodes->ExtractStatus()},
          {"services", _services->ExtractStatus()},
          {"ourKey", ourKey.ToHex()}};
      return obj;
    }

    void
    Context::Init(const Key_t& us, AbstractRouter* r)
    {
      router = r;
      ourKey = us;
      _nodes = std::make_unique<Bucket<RCNode>>(ourKey, llarp::randint);
      _services = std::make_unique<Bucket<ISNode>>(ourKey, llarp::randint);
      llarp::LogDebug("initialize dht with key ", ourKey);
      // start cleanup timer
      _timer_keepalive = std::make_shared<int>(0);
      router->loop()->call_every(1s, _timer_keepalive, [this] { handle_cleaner_timer(); });
    }

    void
    Context::DHTSendTo(const RouterID& peer, IMessage* msg, bool)
    {
      llarp::DHTImmediateMessage m;
      m.msgs.emplace_back(msg);
      router->SendToOrQueue(peer, m);
      auto now = Now();
      router->PersistSessionUntil(peer, now + 1min);
    }

    // this function handles incoming DHT messages sent down a path by a client
    // note that IMessage here is different than that found in the routing
    // namespace. by the time this is called, we are inside
    // llarp::routing::DHTMessage::HandleMessage()
    bool
    Context::RelayRequestForPath(const llarp::PathID_t& id, const IMessage& msg)
    {
      llarp::routing::DHTMessage reply;
      if (!msg.HandleMessage(router->dht(), reply.M))
        return false;
      if (not reply.M.empty())
      {
        auto path = router->pathContext().GetByUpstream(router->pubkey(), id);
        return path && path->SendRoutingMessage(reply, router);
      }
      return true;
    }

    void
    Context::LookupIntroSetForPath(
        const Key_t& addr,
        uint64_t txid,
        const llarp::PathID_t& path,
        const Key_t& askpeer,
        uint64_t relayOrder)
    {
      const TXOwner asker(OurKey(), txid);
      const TXOwner peer(askpeer, ++ids);
      _pendingIntrosetLookups.NewTX(
          peer,
          asker,
          asker,
          new LocalServiceAddressLookup(path, txid, relayOrder, addr, this, askpeer));
    }

    void
    Context::PropagateIntroSetTo(
        const Key_t& from,
        uint64_t txid,
        const service::EncryptedIntroSet& introset,
        const Key_t& tellpeer,
        uint64_t relayOrder)
    {
      const TXOwner asker(from, txid);
      const TXOwner peer(tellpeer, ++ids);
      _pendingIntrosetLookups.NewTX(
          peer, asker, asker, new PublishServiceJob(asker, introset, this, relayOrder));
    }

    void
    Context::PropagateLocalIntroSet(
        const PathID_t& from,
        uint64_t txid,
        const service::EncryptedIntroSet& introset,
        const Key_t& tellpeer,
        uint64_t relayOrder)
    {
      const TXOwner asker(OurKey(), txid);
      const TXOwner peer(tellpeer, ++ids);
      _pendingIntrosetLookups.NewTX(
          peer,
          asker,
          peer,
          new LocalPublishServiceJob(peer, from, txid, introset, this, relayOrder));
    }

    void
    Context::LookupIntroSetRelayed(
        const Key_t& addr,
        const Key_t& whoasked,
        uint64_t txid,
        const Key_t& askpeer,
        uint64_t relayOrder,
        service::EncryptedIntroSetLookupHandler handler)
    {
      const TXOwner asker(whoasked, txid);
      const TXOwner peer(askpeer, ++ids);
      _pendingIntrosetLookups.NewTX(
          peer, asker, asker, new ServiceAddressLookup(asker, addr, this, relayOrder, handler));
    }

    void
    Context::LookupIntroSetDirect(
        const Key_t& addr,
        const Key_t& whoasked,
        uint64_t txid,
        const Key_t& askpeer,
        service::EncryptedIntroSetLookupHandler handler)
    {
      const TXOwner asker(whoasked, txid);
      const TXOwner peer(askpeer, ++ids);
      _pendingIntrosetLookups.NewTX(
          peer, asker, asker, new ServiceAddressLookup(asker, addr, this, 0, handler), 1s);
    }

    bool
    Context::HandleExploritoryRouterLookup(
        const Key_t& requester,
        uint64_t txid,
        const RouterID& target,
        std::vector<std::unique_ptr<IMessage>>& reply)
    {
      std::vector<RouterID> closer;
      const Key_t t(target.as_array());
      std::set<Key_t> foundRouters;
      if (!_nodes)
        return false;

      const size_t nodeCount = _nodes->size();
      if (nodeCount == 0)
      {
        llarp::LogError("cannot handle exploritory router lookup, no dht peers");
        return false;
      }
      llarp::LogDebug("We have ", _nodes->size(), " connected nodes into the DHT");
      // ourKey should never be in the connected list
      // requester is likely in the connected list
      // 4 or connection nodes (minus a potential requestor), whatever is less
      if (!_nodes->GetManyNearExcluding(
              t, foundRouters, std::min(nodeCount, size_t{4}), std::set<Key_t>{ourKey, requester}))
      {
        llarp::LogError(
            "not enough dht nodes to handle exploritory router lookup, "
            "have ",
            nodeCount,
            " dht peers");
        return false;
      }
      for (const auto& f : foundRouters)
      {
        const RouterID id = f.as_array();
        // discard shit routers
        if (router->routerProfiling().IsBadForConnect(id))
          continue;
        closer.emplace_back(id);
      }
      llarp::LogDebug("Gave ", closer.size(), " routers for exploration");
      reply.emplace_back(new GotRouterMessage(txid, closer, false));
      return true;
    }

    void
    Context::LookupRouterForPath(
        const RouterID& target, uint64_t txid, const llarp::PathID_t& path, const Key_t& askpeer)

    {
      const TXOwner peer(askpeer, ++ids);
      const TXOwner whoasked(OurKey(), txid);
      _pendingRouterLookups.NewTX(
          peer, whoasked, target, new LocalRouterLookup(path, txid, target, this));
    }

    void
    Context::LookupRouterRecursive(
        const RouterID& target,
        const Key_t& whoasked,
        uint64_t txid,
        const Key_t& askpeer,
        RouterLookupHandler handler)
    {
      const TXOwner asker(whoasked, txid);
      const TXOwner peer(askpeer, ++ids);
      _pendingRouterLookups.NewTX(
          peer, asker, target, new RecursiveRouterLookup(asker, target, this, handler));
    }

    llarp_time_t
    Context::Now() const
    {
      return router->Now();
    }

    std::unique_ptr<AbstractContext>
    makeContext()
    {
      return std::make_unique<Context>();
    }

  }  // namespace dht
}  // namespace llarp
