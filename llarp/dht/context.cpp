#include <dht/context.hpp>

#include <dht/explorenetworkjob.hpp>
#include <dht/localrouterlookup.hpp>
#include <dht/localserviceaddresslookup.hpp>
#include <dht/localtaglookup.hpp>
#include <dht/messages/findrouter.hpp>
#include <dht/messages/gotintro.hpp>
#include <dht/messages/gotrouter.hpp>
#include <dht/messages/pubintro.hpp>
#include <dht/node.hpp>
#include <dht/publishservicejob.hpp>
#include <dht/recursiverouterlookup.hpp>
#include <dht/serviceaddresslookup.hpp>
#include <dht/taglookup.hpp>
#include <messages/dht_immediate.hpp>
#include <path/path_context.hpp>
#include <router/abstractrouter.hpp>
#include <routing/dht_message.hpp>
#include <util/thread/logic.hpp>
#include <nodedb.hpp>
#include <profiling.hpp>

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
        GetRouter()->nodedb()->InsertAsync(rc);
      }

      /// on behalf of whoasked request introset for target from dht router with
      /// key askpeer
      void
      LookupIntroSetRecursive(
          const service::Address& target, const Key_t& whoasked,
          uint64_t whoaskedTX, const Key_t& askpeer, uint64_t R,
          service::IntroSetLookupHandler result = nullptr) override;

      void
      LookupIntroSetIterative(
          const service::Address& target, const Key_t& whoasked,
          uint64_t whoaskedTX, const Key_t& askpeer,
          service::IntroSetLookupHandler result = nullptr) override;

      /// on behalf of whoasked request router with public key target from dht
      /// router with key askpeer
      void
      LookupRouterRecursive(const RouterID& target, const Key_t& whoasked,
                            uint64_t whoaskedTX, const Key_t& askpeer,
                            RouterLookupHandler result = nullptr) override;

      bool
      LookupRouter(const RouterID& target, RouterLookupHandler result) override
      {
        Key_t askpeer;
        if(!_nodes->FindClosest(Key_t(target), askpeer))
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

      /// on behalf of whoasked request introsets with tag from dht router with
      /// key askpeer with Recursion depth R
      void
      LookupTagRecursive(const service::Tag& tag, const Key_t& whoasked,
                         uint64_t whoaskedTX, const Key_t& askpeer,
                         uint64_t R) override;

      /// issue dht lookup for tag via askpeer and send reply to local path
      void
      LookupTagForPath(const service::Tag& tag, uint64_t txid,
                       const llarp::PathID_t& path,
                       const Key_t& askpeer) override;

      /// issue dht lookup for router via askpeer and send reply to local path
      void
      LookupRouterForPath(const RouterID& target, uint64_t txid,
                          const PathID_t& path, const Key_t& askpeer) override;

      /// issue dht lookup for introset for addr via askpeer and send reply to
      /// local path
      void
      LookupIntroSetForPath(const service::Address& addr, uint64_t txid,
                            const llarp::PathID_t& path,
                            const Key_t& askpeer) override;

      /// send a dht message to peer, if keepalive is true then keep the session
      /// with that peer alive for 10 seconds
      void
      DHTSendTo(const RouterID& peer, IMessage* msg,
                bool keepalive = true) override;

      /// get routers closest to target excluding requester
      bool
      HandleExploritoryRouterLookup(
          const Key_t& requester, uint64_t txid, const RouterID& target,
          std::vector< std::unique_ptr< IMessage > >& reply) override;

      std::set< service::IntroSet >
      FindRandomIntroSetsWithTagExcluding(
          const service::Tag& tag, size_t max = 2,
          const std::set< service::IntroSet >& excludes = {}) override;

      /// handle rc lookup from requester for target
      void
      LookupRouterRelayed(
          const Key_t& requester, uint64_t txid, const Key_t& target,
          bool recursive,
          std::vector< std::unique_ptr< IMessage > >& replies) override;

      /// relay a dht message from a local path to the main network
      bool
      RelayRequestForPath(const llarp::PathID_t& localPath,
                          const IMessage& msg) override;

      /// send introset to peer from source with S counter and excluding peers
      void
      PropagateIntroSetTo(const Key_t& source, uint64_t sourceTX,
                          const service::IntroSet& introset, const Key_t& peer,
                          uint64_t S,
                          const std::set< Key_t >& exclude) override;

      /// initialize dht context and explore every exploreInterval milliseconds
      void
      Init(const Key_t& us, AbstractRouter* router,
           llarp_time_t exploreInterval) override;

      /// get localally stored introset by service address
      const llarp::service::IntroSet*
      GetIntroSetByServiceAddress(
          const llarp::service::Address& addr) const override;

      void
      handle_cleaner_timer(uint64_t interval);

      void
      handle_explore_timer(uint64_t interval);

      /// explore dht for new routers
      void
      Explore(size_t N = 3);

      llarp::AbstractRouter* router{nullptr};
      // for router contacts
      std::unique_ptr< Bucket< RCNode > > _nodes;

      // for introduction sets
      std::unique_ptr< Bucket< ISNode > > _services;

      Bucket< ISNode >*
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

      Bucket< RCNode >*
      Nodes() const override
      {
        return _nodes.get();
      }

      void
      PutRCNodeAsync(const RCNode& val) override
      {
        auto func = std::bind(&Bucket< RCNode >::PutNode, Nodes(), val);
        LogicCall(router->logic(), func);
      }

      void
      DelRCNodeAsync(const Key_t& val) override
      {
        auto func = std::bind(&Bucket< RCNode >::DelNode, Nodes(), val);
        LogicCall(router->logic(), func);
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
        return router->nodedb()->Get(k.as_array(), rc);
      }

      PendingIntrosetLookups _pendingIntrosetLookups;
      PendingTagLookups _pendingTagLookups;
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

      PendingTagLookups&
      pendingTagLookups() override
      {
        return _pendingTagLookups;
      }

      const PendingTagLookups&
      pendingTagLookups() const override
      {
        return _pendingTagLookups;
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
      void
      ScheduleCleanupTimer();

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
      std::set< Key_t > peers;

      if(_nodes->GetManyRandom(peers, N))
      {
        for(const auto& peer : peers)
          ExploreNetworkVia(peer);
      }
      else
        llarp::LogError("failed to select ", N,
                        " random nodes for exploration");
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
    Context::handle_explore_timer(uint64_t interval)
    {
      const auto num = std::min(router->NumberOfConnectedRouters(), size_t(4));
      if(num)
        Explore(num);
      router->logic()->call_later(
          interval,
          std::bind(&llarp::dht::Context::handle_explore_timer, this,
                    interval));
    }

    void
    Context::handle_cleaner_timer(__attribute__((unused)) uint64_t interval)
    {
      // clean up transactions
      CleanupTX();

      if(_services)
      {
        // expire intro sets
        auto now    = Now();
        auto& nodes = _services->nodes;
        auto itr    = nodes.begin();
        while(itr != nodes.end())
        {
          if(itr->second.introset.IsExpired(now))
          {
            llarp::LogDebug("introset expired ", itr->second.introset.A.Addr());
            itr = nodes.erase(itr);
          }
          else
            ++itr;
        }
      }
      ScheduleCleanupTimer();
    }

    std::set< service::IntroSet >
    Context::FindRandomIntroSetsWithTagExcluding(
        const service::Tag& tag, size_t max,
        const std::set< service::IntroSet >& exclude)
    {
      std::set< service::IntroSet > found;
      auto& nodes = _services->nodes;
      if(nodes.size() == 0)
      {
        return found;
      }
      auto itr = nodes.begin();
      // start at random middle point
      auto start = llarp::randint() % nodes.size();
      std::advance(itr, start);
      auto end            = itr;
      std::string tagname = tag.ToString();
      while(itr != nodes.end())
      {
        if(itr->second.introset.topic.ToString() == tagname)
        {
          if(exclude.count(itr->second.introset) == 0)
          {
            found.insert(itr->second.introset);
            if(found.size() == max)
              return found;
          }
        }
        ++itr;
      }
      itr = nodes.begin();
      while(itr != end)
      {
        if(itr->second.introset.topic.ToString() == tagname)
        {
          if(exclude.count(itr->second.introset) == 0)
          {
            found.insert(itr->second.introset);
            if(found.size() == max)
              return found;
          }
        }
        ++itr;
      }
      return found;
    }

    void
    Context::LookupRouterRelayed(
        const Key_t& requester, uint64_t txid, const Key_t& target,
        bool recursive, std::vector< std::unique_ptr< IMessage > >& replies)
    {
      if(target == ourKey)
      {
        // we are the target, give them our RC
        replies.emplace_back(
            new GotRouterMessage(requester, txid, {router->rc()}, false));
        return;
      }
      Key_t next;
      std::set< Key_t > excluding = {requester, ourKey};
      if(_nodes->FindCloseExcluding(target, next, excluding))
      {
        if(next == target)
        {
          // we know it, ask them directly for their own RC to keep it updated
          LookupRouterRecursive(target.as_array(), requester, txid, next);
        }
        else if(recursive)  // are we doing a recursive lookup?
        {
          // is the next peer we ask closer to the target than us?
          if((next ^ target) < (ourKey ^ target))
          {
            // yes it is closer, ask neighbour recursively
            LookupRouterRecursive(target.as_array(), requester, txid, next);
          }
          else
          {
            // no we are closer to the target so tell requester it's not there
            // so they switch to iterative lookup
            replies.emplace_back(
                new GotRouterMessage(requester, txid, {}, false));
          }
        }
        else
        {
          // iterative lookup and we don't have it tell them who is closer
          replies.emplace_back(
              new GotRouterMessage(requester, next, txid, false));
        }
      }
      else
      {
        // we don't know it and have no closer peers to ask
        replies.emplace_back(new GotRouterMessage(requester, txid, {}, false));
      }
    }

    const llarp::service::IntroSet*
    Context::GetIntroSetByServiceAddress(
        const llarp::service::Address& addr) const
    {
      auto key = addr.ToKey();
      auto itr = _services->nodes.find(key);
      if(itr == _services->nodes.end())
        return nullptr;
      return &itr->second.introset;
    }

    void
    Context::CleanupTX()
    {
      auto now = Now();
      llarp::LogDebug("DHT tick");

      pendingRouterLookups().Expire(now);
      _pendingIntrosetLookups.Expire(now);
      pendingTagLookups().Expire(now);
      pendingExploreLookups().Expire(now);
    }

    util::StatusObject
    Context::ExtractStatus() const
    {
      util::StatusObject obj{
          {"pendingRouterLookups", pendingRouterLookups().ExtractStatus()},
          {"pendingIntrosetLookups", _pendingIntrosetLookups.ExtractStatus()},
          {"pendingTagLookups", pendingTagLookups().ExtractStatus()},
          {"pendingExploreLookups", pendingExploreLookups().ExtractStatus()},
          {"nodes", _nodes->ExtractStatus()},
          {"services", _services->ExtractStatus()},
          {"ourKey", ourKey.ToHex()}};
      return obj;
    }

    void
    Context::Init(const Key_t& us, AbstractRouter* r,
                  llarp_time_t exploreInterval)
    {
      router    = r;
      ourKey    = us;
      _nodes    = std::make_unique< Bucket< RCNode > >(ourKey, llarp::randint);
      _services = std::make_unique< Bucket< ISNode > >(ourKey, llarp::randint);
      llarp::LogDebug("initialize dht with key ", ourKey);
      // start exploring

      r->logic()->call_later(
          exploreInterval,
          std::bind(&llarp::dht::Context::handle_explore_timer, this,
                    exploreInterval));
      // start cleanup timer
      ScheduleCleanupTimer();
    }

    void
    Context::ScheduleCleanupTimer()
    {
      router->logic()->call_later(
          1000,
          std::bind(&llarp::dht::Context::handle_cleaner_timer, this, 1000));
    }

    void
    Context::DHTSendTo(const RouterID& peer, IMessage* msg, bool)
    {
      llarp::DHTImmediateMessage m;
      m.msgs.emplace_back(msg);
      router->SendToOrQueue(peer, &m);
      auto now = Now();
      router->PersistSessionUntil(peer, now + 60000);
    }

    bool
    Context::RelayRequestForPath(const llarp::PathID_t& id, const IMessage& msg)
    {
      llarp::routing::DHTMessage reply;
      if(!msg.HandleMessage(router->dht(), reply.M))
        return false;
      if(!reply.M.empty())
      {
        auto path = router->pathContext().GetByUpstream(router->pubkey(), id);
        return path && path->SendRoutingMessage(reply, router);
      }
      return true;
    }

    void
    Context::LookupIntroSetForPath(const service::Address& addr, uint64_t txid,
                                   const llarp::PathID_t& path,
                                   const Key_t& askpeer)
    {
      TXOwner asker(OurKey(), txid);
      TXOwner peer(askpeer, ++ids);
      _pendingIntrosetLookups.NewTX(
          peer, asker, addr,
          new LocalServiceAddressLookup(path, txid, addr, this, askpeer));
    }

    void
    Context::PropagateIntroSetTo(const Key_t& from, uint64_t txid,
                                 const service::IntroSet& introset,
                                 const Key_t& tellpeer, uint64_t S,
                                 const std::set< Key_t >& exclude)
    {
      TXOwner asker(from, txid);
      TXOwner peer(tellpeer, ++ids);
      service::Address addr = introset.A.Addr();
      _pendingIntrosetLookups.NewTX(
          peer, asker, addr,
          new PublishServiceJob(asker, introset, this, S, exclude));
    }

    void
    Context::LookupIntroSetRecursive(const service::Address& addr,
                                     const Key_t& whoasked, uint64_t txid,
                                     const Key_t& askpeer, uint64_t R,
                                     service::IntroSetLookupHandler handler)
    {
      TXOwner asker(whoasked, txid);
      TXOwner peer(askpeer, ++ids);
      _pendingIntrosetLookups.NewTX(
          peer, asker, addr,
          new ServiceAddressLookup(asker, addr, this, R, handler),
          ((R + 1) * 2000));
    }

    void
    Context::LookupIntroSetIterative(const service::Address& addr,
                                     const Key_t& whoasked, uint64_t txid,
                                     const Key_t& askpeer,
                                     service::IntroSetLookupHandler handler)
    {
      TXOwner asker(whoasked, txid);
      TXOwner peer(askpeer, ++ids);
      _pendingIntrosetLookups.NewTX(
          peer, asker, addr,
          new ServiceAddressLookup(asker, addr, this, 0, handler), 1000);
    }

    void
    Context::LookupTagRecursive(const service::Tag& tag, const Key_t& whoasked,
                                uint64_t whoaskedTX, const Key_t& askpeer,
                                uint64_t R)
    {
      TXOwner asker(whoasked, whoaskedTX);
      TXOwner peer(askpeer, ++ids);
      _pendingTagLookups.NewTX(peer, asker, tag,
                               new TagLookup(asker, tag, this, R));
      llarp::LogDebug("ask ", askpeer.SNode(), " for ", tag, " on behalf of ",
                      whoasked.SNode(), " R=", R);
    }

    void
    Context::LookupTagForPath(const service::Tag& tag, uint64_t txid,
                              const llarp::PathID_t& path, const Key_t& askpeer)
    {
      TXOwner peer(askpeer, ++ids);
      TXOwner whoasked(OurKey(), txid);
      _pendingTagLookups.NewTX(peer, whoasked, tag,
                               new LocalTagLookup(path, txid, tag, this));
    }

    bool
    Context::HandleExploritoryRouterLookup(
        const Key_t& requester, uint64_t txid, const RouterID& target,
        std::vector< std::unique_ptr< IMessage > >& reply)
    {
      std::vector< RouterID > closer;
      const Key_t t(target.as_array());
      std::set< Key_t > foundRouters;
      if(!_nodes)
        return false;

      const size_t nodeCount = _nodes->size();
      if(nodeCount == 0)
      {
        llarp::LogError(
            "cannot handle exploritory router lookup, no dht peers");
        return false;
      }
      llarp::LogDebug("We have ", _nodes->size(),
                      " connected nodes into the DHT");
      // ourKey should never be in the connected list
      // requester is likely in the connected list
      // 4 or connection nodes (minus a potential requestor), whatever is less
      if(!_nodes->GetManyNearExcluding(t, foundRouters,
                                       std::min(nodeCount, size_t{4}),
                                       std::set< Key_t >{ourKey, requester}))
      {
        llarp::LogError(
            "not enough dht nodes to handle exploritory router lookup, "
            "have ",
            nodeCount, " dht peers");
        return false;
      }
      for(const auto& f : foundRouters)
      {
        const RouterID r = f.as_array();
        // discard shit routers
        if(router->routerProfiling().IsBadForConnect(r))
          continue;
        closer.emplace_back(r);
      }
      llarp::LogDebug("Gave ", closer.size(), " routers for exploration");
      reply.emplace_back(new GotRouterMessage(txid, closer, false));
      return true;
    }

    void
    Context::LookupRouterForPath(const RouterID& target, uint64_t txid,
                                 const llarp::PathID_t& path,
                                 const Key_t& askpeer)

    {
      const TXOwner peer(askpeer, ++ids);
      const TXOwner whoasked(OurKey(), txid);
      _pendingRouterLookups.NewTX(
          peer, whoasked, target,
          new LocalRouterLookup(path, txid, target, this));
    }

    void
    Context::LookupRouterRecursive(const RouterID& target,
                                   const Key_t& whoasked, uint64_t txid,
                                   const Key_t& askpeer,
                                   RouterLookupHandler handler)
    {
      const TXOwner asker(whoasked, txid);
      const TXOwner peer(askpeer, ++ids);
      _pendingRouterLookups.NewTX(
          peer, asker, target,
          new RecursiveRouterLookup(asker, target, this, handler));
    }

    llarp_time_t
    Context::Now() const
    {
      return router->Now();
    }

    std::unique_ptr< AbstractContext >
    makeContext()
    {
      return std::make_unique< Context >();
    }

  }  // namespace dht
}  // namespace llarp
