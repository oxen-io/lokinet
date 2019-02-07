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
#include <messages/dht.hpp>
#include <messages/dht_immediate.hpp>
#include <router/router.hpp>

#include <vector>

namespace llarp
{
  namespace dht
  {
    AbstractContext::~AbstractContext()
    {
    }

    Context::Context() : router(nullptr), allowTransit(false)
    {
      randombytes((byte_t *)&ids, sizeof(uint64_t));
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
      ctx->router->logic()->call_later({orig, ctx, &handle_explore_timer});
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
      nodes    = std::make_unique< Bucket< RCNode > >(ourKey, llarp::randint);
      services = std::make_unique< Bucket< ISNode > >(ourKey, llarp::randint);
      llarp::LogDebug("initialize dht with key ", ourKey);
      // start exploring

      r->logic()->call_later(
          {exploreInterval, this, &llarp::dht::Context::handle_explore_timer});
      // start cleanup timer
      ScheduleCleanupTimer();
    }

    void
    Context::ScheduleCleanupTimer()
    {
      router->logic()->call_later({1000, this, &handle_cleaner_timer});
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
      if(!msg->HandleMessage(router->dht(), reply.M))
        return false;
      if(!reply.M.empty())
      {
        auto path = router->pathContext().GetByUpstream(router->pubkey(), id);
        return path && path->SendRoutingMessage(&reply, router);
      }
      return true;
    }

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
                                     service::IntroSetLookupHandler handler)
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
                                     service::IntroSetLookupHandler handler)
    {
      TXOwner asker(whoasked, txid);
      TXOwner peer(askpeer, ++ids);
      pendingIntrosetLookups.NewTX(
          peer, asker, addr,
          new ServiceAddressLookup(asker, addr, this, 0, handler));
    }

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
    Context::Crypto() const
    {
      return router->crypto();
    }

    llarp_time_t
    Context::Now() const
    {
      return router->Now();
    }

  }  // namespace dht
}  // namespace llarp
