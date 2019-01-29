#include <dht/context.hpp>

#include <dht/messages/findrouter.hpp>
#include <dht/messages/gotintro.hpp>
#include <dht/messages/gotrouter.hpp>
#include <dht/messages/pubintro.hpp>
#include <dht/node.hpp>
#include <messages/dht.hpp>
#include <messages/dht_immediate.hpp>
#include <router/router.hpp>

#include <vector>

namespace llarp
{
  namespace dht
  {
    Context::Context()
    {
      randombytes((byte_t *)&ids, sizeof(uint64_t));
      allowTransit = false;
    }

    Context::~Context()
    {
      if(nodes)
        delete nodes;
      if(services)
        delete services;
    }

    void
    Context::Explore(size_t N)
    {
      // ask N random peers for new routers
      llarp::LogInfo("Exploring network via ", N, " peers");
      std::set< Key_t > peers;

      if(nodes->GetManyRandom(peers, N))
      {
        for(const auto &peer : peers)
          ExploreNetworkVia(peer);
      }
      else
        llarp::LogError("failed to select random nodes for exploration");
    }

    struct ExploreNetworkJob : public TX< RouterID, RouterID >
    {
      ExploreNetworkJob(const RouterID &peer, Context *ctx)
          : TX< RouterID, RouterID >(TXOwner{}, peer, ctx)
      {
      }

      bool
      Validate(const RouterID &) const override
      {
        // TODO: check with lokid
        return true;
      }

      void
      Start(const TXOwner &peer) override
      {
        parent->DHTSendTo(peer.node.as_array(),
                          new FindRouterMessage(peer.txid));
      }

      bool
      GetNextPeer(Key_t &, const std::set< Key_t > &) override
      {
        return false;
      }

      void
      DoNextRequest(const Key_t &) override
      {
      }

      void
      SendReply() override
      {
        llarp::LogInfo("got ", valuesFound.size(), " routers from exploration");
        for(const auto &pk : valuesFound)
        {
          // lookup router
          parent->LookupRouter(
              pk,
              std::bind(&llarp::Router::HandleDHTLookupForExplore,
                        parent->router, pk, std::placeholders::_1));
        }
      }
    };

    void
    Context::ExploreNetworkVia(const Key_t &askpeer)
    {
      uint64_t txid = ++ids;
      TXOwner peer(askpeer, txid);
      TXOwner whoasked(OurKey(), txid);
      pendingExploreLookups.NewTX(
          peer, whoasked, askpeer.as_array(),
          new ExploreNetworkJob(askpeer.as_array(), this));
    }

    void
    Context::handle_explore_timer(void *u, uint64_t orig, uint64_t left)
    {
      if(left)
        return;
      Context *ctx = static_cast< Context * >(u);
      ctx->Explore(1);
      ctx->router->logic->call_later({orig, ctx, &handle_explore_timer});
    }

    void
    Context::handle_cleaner_timer(void *u,
                                  __attribute__((unused)) uint64_t orig,
                                  uint64_t left)
    {
      if(left)
        return;
      Context *ctx = static_cast< Context * >(u);
      // clean up transactions
      ctx->CleanupTX();

      if(ctx->services)
      {
        // expire intro sets
        auto now    = ctx->Now();
        auto &nodes = ctx->services->nodes;
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
      ctx->ScheduleCleanupTimer();
    }

    std::set< service::IntroSet >
    Context::FindRandomIntroSetsWithTagExcluding(
        const service::Tag &tag, size_t max,
        const std::set< service::IntroSet > &exclude)
    {
      std::set< service::IntroSet > found;
      auto &nodes = services->nodes;
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
        const Key_t &requester, uint64_t txid, const Key_t &target,
        bool recursive, std::vector< std::unique_ptr< IMessage > > &replies)
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
      if(nodes->FindCloseExcluding(target, next, excluding))
      {
        if(next == target)
        {
          // we know it
          replies.emplace_back(new GotRouterMessage(
              requester, txid, {nodes->nodes[target].rc}, false));
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

    const llarp::service::IntroSet *
    Context::GetIntroSetByServiceAddress(
        const llarp::service::Address &addr) const
    {
      auto key = addr.ToKey();
      auto itr = services->nodes.find(key);
      if(itr == services->nodes.end())
        return nullptr;
      return &itr->second.introset;
    }

    void
    Context::CleanupTX()
    {
      auto now = Now();
      llarp::LogDebug("DHT tick");

      pendingRouterLookups.Expire(now);
      pendingIntrosetLookups.Expire(now);
      pendingTagLookups.Expire(now);
      pendingExploreLookups.Expire(now);
    }

    void
    Context::Init(const Key_t &us, llarp::Router *r,
                  llarp_time_t exploreInterval)
    {
      router   = r;
      ourKey   = us;
      nodes    = new Bucket< RCNode >(ourKey, llarp::randint);
      services = new Bucket< ISNode >(ourKey, llarp::randint);
      llarp::LogDebug("initialize dht with key ", ourKey);
      // start exploring

      r->logic->call_later(
          {exploreInterval, this, &llarp::dht::Context::handle_explore_timer});
      // start cleanup timer
      ScheduleCleanupTimer();
    }

    void
    Context::ScheduleCleanupTimer()
    {
      router->logic->call_later({1000, this, &handle_cleaner_timer});
    }

    void
    Context::DHTSendTo(const RouterID &peer, IMessage *msg, bool keepalive)
    {
      llarp::DHTImmediateMessage m;
      m.msgs.emplace_back(msg);
      router->SendToOrQueue(peer, &m);
      if(keepalive)
      {
        auto now = Now();
        router->PersistSessionUntil(peer, now + 10000);
      }
    }

    bool
    Context::RelayRequestForPath(const llarp::PathID_t &id, const IMessage *msg)
    {
      llarp::routing::DHTMessage reply;
      if(!msg->HandleMessage(router->dht, reply.M))
        return false;
      if(reply.M.size())
      {
        auto path = router->paths.GetByUpstream(router->pubkey(), id);
        return path && path->SendRoutingMessage(&reply, router);
      }
      return true;
    }

    struct ServiceAddressLookup
        : public TX< service::Address, service::IntroSet >
    {
      IntroSetLookupHandler handleResult;
      uint64_t R;

      ServiceAddressLookup(const TXOwner &asker, const service::Address &addr,
                           Context *ctx, uint64_t r,
                           IntroSetLookupHandler handler)
          : TX< service::Address, service::IntroSet >(asker, addr, ctx)
          , handleResult(handler)
          , R(r)
      {
        peersAsked.insert(ctx->OurKey());
      }

      bool
      Validate(const service::IntroSet &value) const override
      {
        if(!value.Verify(parent->Crypto(), parent->Now()))
        {
          llarp::LogWarn("Got invalid introset from service lookup");
          return false;
        }
        if(value.A.Addr() != target)
        {
          llarp::LogWarn("got introset with wrong target from service lookup");
          return false;
        }
        return true;
      }

      bool
      GetNextPeer(Key_t &next, const std::set< Key_t > &exclude) override
      {
        Key_t k = target.ToKey();
        return parent->nodes->FindCloseExcluding(k, next, exclude);
      }

      void
      Start(const TXOwner &peer) override
      {
        parent->DHTSendTo(peer.node.as_array(),
                          new FindIntroMessage(peer.txid, target, R));
      }

      void
      DoNextRequest(const Key_t &ask) override
      {
        if(R)
          parent->LookupIntroSetRecursive(target, whoasked.node, whoasked.txid,
                                          ask, R - 1);
        else
          parent->LookupIntroSetIterative(target, whoasked.node, whoasked.txid,
                                          ask);
      }

      virtual void
      SendReply() override
      {
        if(handleResult)
          handleResult(valuesFound);

        parent->DHTSendTo(whoasked.node.as_array(),
                          new GotIntroMessage(valuesFound, whoasked.txid));
      }
    };

    struct LocalServiceAddressLookup : public ServiceAddressLookup
    {
      PathID_t localPath;

      LocalServiceAddressLookup(const PathID_t &pathid, uint64_t txid,
                                const service::Address &addr, Context *ctx,
                                __attribute__((unused)) const Key_t &askpeer)
          : ServiceAddressLookup(TXOwner{ctx->OurKey(), txid}, addr, ctx, 5,
                                 nullptr)
          , localPath(pathid)
      {
      }

      void
      SendReply() override
      {
        auto path = parent->router->paths.GetByUpstream(
            parent->OurKey().as_array(), localPath);
        if(!path)
        {
          llarp::LogWarn(
              "did not send reply for relayed dht request, no such local path "
              "for pathid=",
              localPath);
          return;
        }
        routing::DHTMessage msg;
        msg.M.emplace_back(new GotIntroMessage(valuesFound, whoasked.txid));
        if(!path->SendRoutingMessage(&msg, parent->router))
        {
          llarp::LogWarn(
              "failed to send routing message when informing result of dht "
              "request, pathid=",
              localPath);
        }
      }
    };

    void
    Context::LookupIntroSetForPath(const service::Address &addr, uint64_t txid,
                                   const llarp::PathID_t &path,
                                   const Key_t &askpeer)
    {
      TXOwner asker(OurKey(), txid);
      TXOwner peer(askpeer, ++ids);
      pendingIntrosetLookups.NewTX(
          peer, asker, addr,
          new LocalServiceAddressLookup(path, txid, addr, this, askpeer));
    }

    struct PublishServiceJob : public TX< service::Address, service::IntroSet >
    {
      uint64_t S;
      std::set< Key_t > dontTell;
      service::IntroSet I;
      PublishServiceJob(const TXOwner &asker, const service::IntroSet &introset,
                        Context *ctx, uint64_t s,
                        const std::set< Key_t > &exclude)
          : TX< service::Address, service::IntroSet >(asker, introset.A.Addr(),
                                                      ctx)
          , S(s)
          , dontTell(exclude)
          , I(introset)
      {
      }

      bool
      Validate(const service::IntroSet &introset) const override
      {
        if(I.A != introset.A)
        {
          llarp::LogWarn(
              "publish introset acknowledgement acked a different service");
          return false;
        }
        return true;
      }

      void
      Start(const TXOwner &peer) override
      {
        std::vector< Key_t > exclude;
        for(const auto &router : dontTell)
          exclude.push_back(router);
        parent->DHTSendTo(peer.node.as_array(),
                          new PublishIntroMessage(I, peer.txid, S, exclude));
      }

      bool
      GetNextPeer(Key_t &, const std::set< Key_t > &) override
      {
        return false;
      }

      void
      DoNextRequest(const Key_t &) override
      {
      }

      void
      SendReply() override
      {
        // don't need this
      }
    };

    void
    Context::PropagateIntroSetTo(const Key_t &from, uint64_t txid,
                                 const service::IntroSet &introset,
                                 const Key_t &tellpeer, uint64_t S,
                                 const std::set< Key_t > &exclude)
    {
      TXOwner asker(from, txid);
      TXOwner peer(tellpeer, ++ids);
      service::Address addr = introset.A.Addr();
      pendingIntrosetLookups.NewTX(
          peer, asker, addr,
          new PublishServiceJob(asker, introset, this, S, exclude));
    }

    void
    Context::LookupIntroSetRecursive(const service::Address &addr,
                                     const Key_t &whoasked, uint64_t txid,
                                     const Key_t &askpeer, uint64_t R,
                                     IntroSetLookupHandler handler)
    {
      TXOwner asker(whoasked, txid);
      TXOwner peer(askpeer, ++ids);
      pendingIntrosetLookups.NewTX(
          peer, asker, addr,
          new ServiceAddressLookup(asker, addr, this, R, handler));
    }

    void
    Context::LookupIntroSetIterative(const service::Address &addr,
                                     const Key_t &whoasked, uint64_t txid,
                                     const Key_t &askpeer,
                                     IntroSetLookupHandler handler)
    {
      TXOwner asker(whoasked, txid);
      TXOwner peer(askpeer, ++ids);
      pendingIntrosetLookups.NewTX(
          peer, asker, addr,
          new ServiceAddressLookup(asker, addr, this, 0, handler));
    }

    struct TagLookup : public TX< service::Tag, service::IntroSet >
    {
      uint64_t R;
      TagLookup(const TXOwner &asker, const service::Tag &tag, Context *ctx,
                uint64_t r)
          : TX< service::Tag, service::IntroSet >(asker, tag, ctx), R(r)
      {
      }

      bool
      Validate(const service::IntroSet &introset) const override
      {
        if(!introset.Verify(parent->Crypto(), parent->Now()))
        {
          llarp::LogWarn("got invalid introset from tag lookup");
          return false;
        }
        if(introset.topic != target)
        {
          llarp::LogWarn("got introset with missmatched topic in tag lookup");
          return false;
        }
        return true;
      }

      void
      Start(const TXOwner &peer) override
      {
        parent->DHTSendTo(peer.node.as_array(),
                          new FindIntroMessage(target, peer.txid, R));
      }

      bool
      GetNextPeer(Key_t &, const std::set< Key_t > &) override
      {
        return false;
      }

      void
      DoNextRequest(const Key_t &) override
      {
      }

      void
      SendReply() override
      {
        std::set< service::IntroSet > found;
        for(const auto &remoteTag : valuesFound)
        {
          found.insert(remoteTag);
        }
        // collect our local values if we haven't hit a limit
        if(found.size() < 2)
        {
          for(const auto &localTag :
              parent->FindRandomIntroSetsWithTagExcluding(target, 1, found))
          {
            found.insert(localTag);
          }
        }
        std::vector< service::IntroSet > values;
        for(const auto &introset : found)
        {
          values.push_back(introset);
        }
        parent->DHTSendTo(whoasked.node.as_array(),
                          new GotIntroMessage(values, whoasked.txid));
      }
    };

    void
    Context::LookupTagRecursive(const service::Tag &tag, const Key_t &whoasked,
                                uint64_t whoaskedTX, const Key_t &askpeer,
                                uint64_t R)
    {
      TXOwner asker(whoasked, whoaskedTX);
      TXOwner peer(askpeer, ++ids);
      pendingTagLookups.NewTX(peer, asker, tag,
                              new TagLookup(asker, tag, this, R));
      llarp::LogInfo("ask ", askpeer, " for ", tag, " on behalf of ", whoasked,
                     " R=", R);
    }

    struct LocalTagLookup : public TagLookup
    {
      PathID_t localPath;

      LocalTagLookup(const PathID_t &path, uint64_t txid,
                     const service::Tag &target, Context *ctx)
          : TagLookup(TXOwner{ctx->OurKey(), txid}, target, ctx, 0)
          , localPath(path)
      {
      }

      void
      SendReply() override
      {
        auto path = parent->router->paths.GetByUpstream(
            parent->OurKey().as_array(), localPath);
        if(!path)
        {
          llarp::LogWarn(
              "did not send reply for relayed dht request, no such local path "
              "for pathid=",
              localPath);
          return;
        }
        routing::DHTMessage msg;
        msg.M.emplace_back(new GotIntroMessage(valuesFound, whoasked.txid));
        if(!path->SendRoutingMessage(&msg, parent->router))
        {
          llarp::LogWarn(
              "failed to send routing message when informing result of dht "
              "request, pathid=",
              localPath);
        }
      }
    };

    void
    Context::LookupTagForPath(const service::Tag &tag, uint64_t txid,
                              const llarp::PathID_t &path, const Key_t &askpeer)
    {
      TXOwner peer(askpeer, ++ids);
      TXOwner whoasked(OurKey(), txid);
      pendingTagLookups.NewTX(peer, whoasked, tag,
                              new LocalTagLookup(path, txid, tag, this));
    }

    bool
    Context::HandleExploritoryRouterLookup(
        const Key_t &requester, uint64_t txid, const RouterID &target,
        std::vector< std::unique_ptr< IMessage > > &reply)
    {
      std::vector< RouterID > closer;
      Key_t t(target.as_array());
      std::set< Key_t > found;
      if(!nodes)
        return false;

      size_t nodeCount = nodes->size();
      if(nodeCount == 0)
      {
        llarp::LogError(
            "cannot handle exploritory router lookup, no dht peers");
        return false;
      }
      llarp::LogDebug("We have ", nodes->size(),
                      " connected nodes into the DHT");
      // ourKey should never be in the connected list
      // requester is likely in the connected list
      // 4 or connection nodes (minus a potential requestor), whatever is less
      size_t want = std::min(size_t(4), nodeCount - 1);
      llarp::LogDebug("We want ", want, " connected nodes in the DHT");
      if(!nodes->GetManyNearExcluding(t, found, want,
                                      std::set< Key_t >{ourKey, requester}))
      {
        llarp::LogError(
            "not enough dht nodes to handle exploritory router lookup, "
            "need a minimum of ",
            want, " dht peers");
        return false;
      }
      for(const auto &f : found)
        closer.emplace_back(f.as_array());
      reply.emplace_back(new GotRouterMessage(txid, closer, false));
      return true;
    }

    struct RecursiveRouterLookup : public TX< RouterID, RouterContact >
    {
      RouterLookupHandler resultHandler;
      RecursiveRouterLookup(const TXOwner &whoasked, const RouterID &target,
                            Context *ctx, RouterLookupHandler result)
          : TX< RouterID, RouterContact >(whoasked, target, ctx)
          , resultHandler(result)

      {
        peersAsked.insert(ctx->OurKey());
      }

      bool
      Validate(const RouterContact &rc) const override
      {
        if(!rc.Verify(parent->Crypto(), parent->Now()))
        {
          llarp::LogWarn("rc from lookup result is invalid");
          return false;
        }
        return true;
      }

      bool
      GetNextPeer(Key_t &, const std::set< Key_t > &) override
      {
        return false;
      }

      void
      DoNextRequest(const Key_t &) override
      {
      }

      void
      Start(const TXOwner &peer) override
      {
        parent->DHTSendTo(peer.node.as_array(),
                          new FindRouterMessage(peer.txid, target));
      }

      virtual void
      SendReply() override
      {
        if(resultHandler)
        {
          resultHandler(valuesFound);
        }
        else
        {
          parent->DHTSendTo(
              whoasked.node.as_array(),
              new GotRouterMessage({}, whoasked.txid, valuesFound, false));
        }
      }
    };

    struct LocalRouterLookup : public RecursiveRouterLookup
    {
      PathID_t localPath;

      LocalRouterLookup(const PathID_t &path, uint64_t txid,
                        const RouterID &target, Context *ctx)
          : RecursiveRouterLookup(TXOwner{ctx->OurKey(), txid}, target, ctx,
                                  nullptr)
          , localPath(path)
      {
      }

      void
      SendReply() override
      {
        auto path = parent->router->paths.GetByUpstream(
            parent->OurKey().as_array(), localPath);
        if(!path)
        {
          llarp::LogWarn(
              "did not send reply for relayed dht request, no such local path "
              "for pathid=",
              localPath);
          return;
        }
        routing::DHTMessage msg;
        msg.M.emplace_back(new GotRouterMessage(parent->OurKey(), whoasked.txid,
                                                valuesFound, true));
        if(!path->SendRoutingMessage(&msg, parent->router))
        {
          llarp::LogWarn(
              "failed to send routing message when informing result of dht "
              "request, pathid=",
              localPath);
        }
      }
    };

    void
    Context::LookupRouterForPath(const RouterID &target, uint64_t txid,
                                 const llarp::PathID_t &path,
                                 const Key_t &askpeer)

    {
      TXOwner peer(askpeer, ++ids);
      TXOwner whoasked(OurKey(), txid);
      pendingRouterLookups.NewTX(
          peer, whoasked, target,
          new LocalRouterLookup(path, txid, target, this));
    }

    void
    Context::LookupRouterRecursive(const RouterID &target,
                                   const Key_t &whoasked, uint64_t txid,
                                   const Key_t &askpeer,
                                   RouterLookupHandler handler)
    {
      TXOwner asker(whoasked, txid);
      TXOwner peer(askpeer, ++ids);
      if(target != askpeer)
      {
        pendingRouterLookups.NewTX(
            peer, asker, target,
            new RecursiveRouterLookup(asker, target, this, handler));
      }
    }

    llarp::Crypto *
    Context::Crypto()
    {
      return router->crypto.get();
    }

    llarp_time_t
    Context::Now()
    {
      return llarp_ev_loop_time_now_ms(router->netloop);
    }

  }  // namespace dht
}  // namespace llarp
