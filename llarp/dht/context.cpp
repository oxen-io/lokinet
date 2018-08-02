#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/gotrouter.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/dht_immediate.hpp>
#include <vector>
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
      if(ctx->services)
      {
        // expire intro sets
        auto now    = llarp_time_now_ms();
        auto &nodes = ctx->services->nodes;
        auto itr    = nodes.begin();
        while(itr != nodes.end())
        {
          if(itr->second.introset.IsExpired(now))
            itr = nodes.erase(itr);
          else
            ++itr;
        }
      }
      ctx->ScheduleCleanupTimer();
    }

    struct PathLookupJob
    {
      uint64_t txid;
      PathID_t pathID;
      llarp_router *m_router;
      std::set< service::IntroSet > localIntroSets;
      PathLookupJob(llarp_router *r, const PathID_t &localpath, uint64_t tx)
          : txid(tx), pathID(localpath), m_router(r)
      {
      }

      void
      OnResult(const std::vector< service::IntroSet > &results)
      {
        auto path =
            m_router->paths.GetByUpstream(m_router->dht->impl.OurKey(), pathID);
        if(path)
        {
          for(const auto &introset : results)
          {
            localIntroSets.insert(introset);
          }
          llarp::routing::DHTMessage msg;
          auto sz = localIntroSets.size();
          std::vector< service::IntroSet > intros(sz);
          for(const auto &i : localIntroSets)
          {
            intros[--sz] = i;
          }
          msg.M.push_back(new llarp::dht::GotIntroMessage(intros, txid));
          path->SendRoutingMessage(&msg, m_router);
        }
        else
        {
          llarp::LogWarn("no local path for reply on PathTagLookupJob pathid=",
                         pathID);
        }
        delete this;
      }
    };

    void
    Context::PropagateIntroSetTo(const Key_t &from, uint64_t txid,
                                 const service::IntroSet &introset,
                                 const Key_t &peer, uint64_t S,
                                 const std::set< Key_t > &exclude)
    {
      llarp::LogInfo("Propagate Introset for ", introset.A.Name(), " to ",
                     peer);
      auto id = ++ids;

      std::vector< Key_t > E;
      for(const auto &ex : exclude)
        E.push_back(ex);

      TXOwner ownerKey;
      ownerKey.node = peer;
      ownerKey.txid = id;
      SearchJob job(from, txid,
                    [](const std::vector< service::IntroSet > &) {});
      pendingTX[ownerKey] = job;
      auto msg            = new llarp::DHTImmeidateMessage(peer);
      msg->msgs.push_back(new PublishIntroMessage(introset, id, S, E));
      router->SendToOrQueue(peer, msg);
    }

    void
    Context::LookupTagForPath(const service::Tag &tag, uint64_t txid,
                              const llarp::PathID_t &path, const Key_t &askpeer)
    {
      auto id = ++ids;
      TXOwner ownerKey;
      ownerKey.node     = askpeer;
      ownerKey.txid     = id;
      PathLookupJob *j  = new PathLookupJob(router, path, txid);
      j->localIntroSets = FindRandomIntroSetsWithTag(tag);
      SearchJob job(
          OurKey(), txid,
          std::bind(&PathLookupJob::OnResult, j, std::placeholders::_1));
      pendingTX[ownerKey] = job;

      auto msg    = new llarp::DHTImmeidateMessage(askpeer);
      auto dhtmsg = new FindIntroMessage(tag, id);
      msg->msgs.push_back(dhtmsg);
      router->SendToOrQueue(askpeer, msg);
    }

    void
    Context::LookupIntroSetForPath(const service::Address &addr, uint64_t txid,
                                   const llarp::PathID_t &path,
                                   const Key_t &askpeer)
    {
      auto id = ++ids;
      TXOwner ownerKey;
      ownerKey.node    = askpeer;
      ownerKey.txid    = id;
      PathLookupJob *j = new PathLookupJob(router, path, txid);
      SearchJob job(
          OurKey(), txid,
          std::bind(&PathLookupJob::OnResult, j, std::placeholders::_1));
      pendingTX[ownerKey] = job;

      auto msg    = new llarp::DHTImmeidateMessage(askpeer);
      auto dhtmsg = new FindIntroMessage(addr, id);
      msg->msgs.push_back(dhtmsg);
      router->SendToOrQueue(askpeer, msg);
    }

    std::set< service::IntroSet >
    Context::FindRandomIntroSetsWithTag(const service::Tag &tag, size_t max)
    {
      std::set< service::IntroSet > found;
      auto &nodes = services->nodes;
      if(nodes.size() == 0)
        return found;

      auto itr = nodes.begin();
      // start at random middle point
      auto start = llarp_randint() % nodes.size();
      std::advance(itr, start);
      auto end = itr;
      while(itr != nodes.end())
      {
        if(itr->second.introset.topic == tag)
        {
          found.insert(itr->second.introset);
          if(found.size() == max)
            return found;
        }
        ++itr;
      }
      itr = nodes.begin();
      while(itr != end)
      {
        if(itr->second.introset.topic == tag)
        {
          found.insert(itr->second.introset);
          if(found.size() == max)
            return found;
        }
        ++itr;
      }
      return found;
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

    const llarp::service::IntroSet *
    Context::GetIntroSetByServiceAddress(
        const llarp::service::Address &addr) const
    {
      auto itr = services->nodes.find(addr.data());
      if(itr == services->nodes.end())
        return nullptr;
      return &itr->second.introset;
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

    /// handles replying with a GIM for a lookup
    struct IntroSetInformJob
    {
      std::set< service::IntroSet > localIntroSets;
      Key_t replyNode;
      uint64_t txid;
      llarp_router *m_Router;
      IntroSetInformJob(llarp_router *r, const Key_t &replyTo, uint64_t id)
          : replyNode(replyTo), txid(id), m_Router(r)
      {
      }

      void
      OnResult(const std::vector< llarp::service::IntroSet > &results)
      {
        for(const auto &introset : results)
        {
          localIntroSets.insert(introset);
        }
        if(replyNode != m_Router->dht->impl.OurKey())
        {
          std::vector< service::IntroSet > reply;
          for(const auto &introset : localIntroSets)
          {
            reply.push_back(introset);
          }
          auto msg = new llarp::DHTImmeidateMessage(replyNode);
          msg->msgs.push_back(new GotIntroMessage(reply, txid));
          m_Router->SendToOrQueue(replyNode, msg);
        }
        delete this;
      }
    };

    void
    Context::LookupTag(const llarp::service::Tag &tag, const Key_t &whoasked,
                       uint64_t txid, const Key_t &askpeer,
                       const std::set< service::IntroSet > &include,
                       bool iterative)
    {
      auto id = ++ids;
      if(txid == 0)
        txid = id;
      TXOwner ownerKey;
      ownerKey.node        = askpeer;
      ownerKey.txid        = id;
      IntroSetInformJob *j = new IntroSetInformJob(router, whoasked, txid);
      j->localIntroSets    = include;
      SearchJob job(
          whoasked, txid,
          std::bind(&IntroSetInformJob::OnResult, j, std::placeholders::_1));
      pendingTX[ownerKey] = job;

      auto msg          = new llarp::DHTImmeidateMessage(askpeer);
      auto dhtmsg       = new FindIntroMessage(tag, id);
      dhtmsg->iterative = iterative;
      msg->msgs.push_back(dhtmsg);
      router->SendToOrQueue(askpeer, msg);
    }

    void
    Context::LookupIntroSet(const service::Address &addr, const Key_t &whoasked,
                            uint64_t txid, const Key_t &askpeer, bool iterative,
                            std::set< Key_t > excludes)
    {
      auto id = ++ids;
      if(txid == 0)
        txid = id;

      TXOwner ownerKey;
      ownerKey.node        = askpeer;
      ownerKey.txid        = id;
      IntroSetInformJob *j = new IntroSetInformJob(router, whoasked, txid);
      SearchJob job(
          whoasked, txid, addr, excludes,
          std::bind(&IntroSetInformJob::OnResult, j, std::placeholders::_1));
      pendingTX[ownerKey] = job;

      auto msg    = new llarp::DHTImmeidateMessage(askpeer);
      auto dhtmsg = new FindIntroMessage(addr, id);
      msg->msgs.push_back(dhtmsg);
      router->SendToOrQueue(askpeer, msg);
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
