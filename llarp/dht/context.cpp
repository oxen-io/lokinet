#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/gotrouter.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/dht_immediate.hpp>
#include "router.hpp"

namespace llarp
{
  namespace dht
  {
    Context::Context()
    {
      randombytes((byte_t *)&ids, sizeof(uint64_t));
    }

    Context::~Context()
    {
      if(nodes)
        delete nodes;
      if(services)
        delete services;
    }

    void
    Context::handle_cleaner_timer(void *u, uint64_t orig, uint64_t left)
    {
      if(left)
        return;
      Context *ctx = static_cast< Context * >(u);

      ctx->CleanupTX();
      ctx->ScheduleCleanupTimer();
    }

    void
    Context::LookupRouterRelayed(const Key_t &requester, uint64_t txid,
                                 const Key_t &target, bool recursive,
                                 std::vector< IMessage * > &replies)
    {
      if(target == ourKey)
      {
        // we are the target, give them our RC
        replies.push_back(new GotRouterMessage(requester, txid, &router->rc));
        return;
      }
      Key_t next;
      std::set< Key_t > excluding = {requester, ourKey};
      if(nodes->FindCloseExcluding(target, next, excluding))
      {
        if(next == target)
        {
          // we know it
          replies.push_back(
              new GotRouterMessage(requester, txid, nodes->nodes[target].rc));
        }
        else if(recursive)  // are we doing a recursive lookup?
        {
          if((requester ^ target) < (ourKey ^ target))
          {
            // we aren't closer to the target than next hop
            // so we won't ask neighboor recursively, tell them we don't have it
            llarp::LogInfo("we aren't closer to ", target, " than ", next,
                           " so we end it here");
            replies.push_back(new GotRouterMessage(requester, txid, nullptr));
          }
          else
          {
            // yeah, ask neighboor recursively
            // FIXME: we may need to pass a job here...
            // auto sj = FindPendingTX(requester, txid);
            // LookupRouter(target, requester, txid, next, sj->job);
            LookupRouter(target, requester, txid, next);
          }
        }
        else  // otherwise tell them we don't have it
        {
          llarp::LogInfo("we don't have ", target,
                         " and this was an iterative request so telling ",
                         requester, " that we don't have it");
          replies.push_back(new GotRouterMessage(requester, txid, nullptr));
        }
      }
      else
      {
        // we don't know it and have no closer peers
        llarp::LogInfo("we don't have ", target,
                       " and have no closer peers so telling ", requester,
                       " that we don't have it");
        replies.push_back(new GotRouterMessage(requester, txid, nullptr));
      }
    }

    void
    Context::RemovePendingLookup(const Key_t &owner, uint64_t id)
    {
      TXOwner search;
      search.node = owner;
      search.txid = id;
      auto itr    = pendingTX.find(search);
      if(itr == pendingTX.end())
        return;
      pendingTX.erase(itr);
    }

    SearchJob *
    Context::FindPendingTX(const Key_t &owner, uint64_t id)
    {
      TXOwner search;
      search.node = owner;
      search.txid = id;
      auto itr    = pendingTX.find(search);
      if(itr == pendingTX.end())
        return nullptr;
      else
        return &itr->second;
    }

    void
    Context::CleanupTX()
    {
      auto now = llarp_time_now_ms();
      llarp::LogDebug("DHT tick");

      auto itr = pendingTX.begin();
      while(itr != pendingTX.end())
      {
        if(itr->second.IsExpired(now))
        {
          itr->second.Timeout();
          itr = pendingTX.erase(itr);
        }
        else
          ++itr;
      }
    }

    void
    Context::Init(const Key_t &us, llarp_router *r)
    {
      router   = r;
      ourKey   = us;
      nodes    = new Bucket< RCNode >(ourKey);
      services = new Bucket< ISNode >(ourKey);
      llarp::LogDebug("intialize dht with key ", ourKey);
    }

    void
    Context::ScheduleCleanupTimer()
    {
      llarp_logic_call_later(router->logic,
                             {1000, this, &handle_cleaner_timer});
    }

    bool
    Context::RelayRequestForPath(const llarp::PathID_t &id, const IMessage *msg)
    {
      llarp::routing::DHTMessage reply;
      if(!msg->HandleMessage(router->dht, reply.M))
        return false;
      auto path = router->paths.GetByUpstream(router->pubkey(), id);
      return path && path->SendRoutingMessage(&reply, router);
    }

    void
    Context::LookupRouter(const Key_t &target, const Key_t &whoasked,
                          uint64_t txid, const Key_t &askpeer,
                          llarp_router_lookup_job *job, bool iterative,
                          std::set< Key_t > excludes)
    {
      if(target.IsZero() || whoasked.IsZero() || askpeer.IsZero())
      {
        return;
      }
      auto id = ++ids;
      TXOwner ownerKey;
      ownerKey.node = askpeer;
      ownerKey.txid = id;
      if(txid == 0)
        txid = id;
      SearchJob j(whoasked, txid, target, excludes, job);
      pendingTX[ownerKey] = j;
      llarp::LogInfo("Asking ", askpeer, " for router ", target, " for ",
                     whoasked);
      auto msg          = new llarp::DHTImmeidateMessage(askpeer);
      auto dhtmsg       = new FindRouterMessage(askpeer, target, id);
      dhtmsg->iterative = iterative;
      msg->msgs.push_back(dhtmsg);
      router->SendToOrQueue(askpeer, msg);
    }

    void
    Context::LookupRouterViaJob(llarp_router_lookup_job *job)
    {
      Key_t peer;
      /*
      llarp::LogInfo("LookupRouterViaJob dumping nodes");
      for(const auto &item : nodes->nodes)
      {
        llarp::LogInfo("LookupRouterViaJob dumping node: ", item.first);
      }
      */
      llarp::LogInfo("LookupRouterViaJob node count: ", nodes->nodes.size());
      llarp::LogInfo("LookupRouterViaJob recursive: ",
                     job->iterative ? "yes" : "no");

      if(nodes->FindClosest(job->target, peer))
        LookupRouter(job->target, ourKey, 0, peer, job, job->iterative);
      else if(job->hook)
      {
        job->found = false;
        job->hook(job);
      }
    }

    void
    Context::queue_router_lookup(void *user)
    {
      llarp_router_lookup_job *job =
          static_cast< llarp_router_lookup_job * >(user);
      job->dht->impl.LookupRouterViaJob(job);
    }

  }  // namespace dht
}  // namespace llarp
