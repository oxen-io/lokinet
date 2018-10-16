
#include <llarp/dht/messages/findintro.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/service/endpoint.hpp>
#include <llarp/service/protocol.hpp>
#include "buffer.hpp"
#include "router.hpp"

namespace llarp
{
  namespace service
  {
    Endpoint::Endpoint(const std::string& name, llarp_router* r)
        : path::Builder(r, r->dht, 6, 4), m_Router(r), m_Name(name)
    {
      m_Tag.Zero();
    }

    bool
    Endpoint::SetOption(const std::string& k, const std::string& v)
    {
      if(k == "keyfile")
      {
        m_Keyfile = v;
      }
      if(k == "tag")
      {
        m_Tag = v;
        llarp::LogInfo("Setting tag to ", v);
      }
      if(k == "prefetch-tag")
      {
        m_PrefetchTags.insert(v);
      }
      if(k == "prefetch-addr")
      {
        Address addr;
        if(addr.FromString(v))
          m_PrefetchAddrs.insert(addr);
      }
      if(k == "netns")
      {
        m_NetNS = v;
        m_OnInit.push_back(std::bind(&Endpoint::IsolateNetwork, this));
      }
      if(k == "min-latency")
      {
        auto val = atoi(v.c_str());
        if(val > 0)
          m_MinPathLatency = val;
      }
      return true;
    }

    bool
    Endpoint::IsolateNetwork()
    {
      llarp::LogInfo("isolating network to namespace ", m_NetNS);
      m_IsolatedWorker = llarp_init_isolated_net_threadpool(
          m_NetNS.c_str(), &SetupIsolatedNetwork, &RunIsolatedMainLoop, this);
      m_IsolatedLogic = llarp_init_single_process_logic(m_IsolatedWorker);
      return true;
    }

    llarp_ev_loop*
    Endpoint::EndpointNetLoop()
    {
      if(m_IsolatedNetLoop)
        return m_IsolatedNetLoop;
      else
        return m_Router->netloop;
    }

    bool
    Endpoint::NetworkIsIsolated() const
    {
      return m_IsolatedLogic && m_IsolatedWorker;
    }

    bool
    Endpoint::SetupIsolatedNetwork(void* user, bool failed)
    {
      return static_cast< Endpoint* >(user)->DoNetworkIsolation(!failed);
    }

    bool
    Endpoint::HasPendingPathToService(const Address& addr) const
    {
      return m_PendingServiceLookups.find(addr)
          != m_PendingServiceLookups.end();
    }

    void
    Endpoint::RegenAndPublishIntroSet(llarp_time_t now, bool forceRebuild)
    {
      std::set< Introduction > I;
      if(!GetCurrentIntroductionsWithFilter(
             I, [now](const service::Introduction& intro) -> bool {
               return now < intro.expiresAt
                   && intro.expiresAt - now > (2 * 60 * 1000);
             }))
      {
        llarp::LogWarn("could not publish descriptors for endpoint ", Name(),
                       " because we couldn't get enough valid introductions");
        if(ShouldBuildMore() || forceRebuild)
          ManualRebuild(1);
        return;
      }
      m_IntroSet.I.clear();
      for(const auto& intro : I)
      {
        m_IntroSet.I.push_back(intro);
      }
      if(m_IntroSet.I.size() == 0)
      {
        llarp::LogWarn("not enough intros to publish introset for ", Name());
        return;
      }
      m_IntroSet.topic = m_Tag;
      if(!m_Identity.SignIntroSet(m_IntroSet, &m_Router->crypto))
      {
        llarp::LogWarn("failed to sign introset for endpoint ", Name());
        return;
      }
      if(PublishIntroSet(m_Router))
      {
        llarp::LogInfo("(re)publishing introset for endpoint ", Name());
      }
      else
      {
        llarp::LogWarn("failed to publish intro set for endpoint ", Name());
      }
    }

    void
    Endpoint::Tick(llarp_time_t now)
    {
      // publish descriptors
      if(ShouldPublishDescriptors(now))
      {
        RegenAndPublishIntroSet(now);
      }
      // expire pending tx
      {
        std::set< service::IntroSet > empty;
        auto itr = m_PendingLookups.begin();
        while(itr != m_PendingLookups.end())
        {
          if(itr->second->IsTimedOut(now))
          {
            std::unique_ptr< IServiceLookup > lookup = std::move(itr->second);

            llarp::LogInfo(lookup->name, " timed out txid=", lookup->txid);
            lookup->HandleResponse(empty);
            itr = m_PendingLookups.erase(itr);
          }
          else
            ++itr;
        }
      }

      // expire pending router lookups
      {
        auto itr = m_PendingRouters.begin();
        while(itr != m_PendingRouters.end())
        {
          if(itr->second.IsExpired(now))
          {
            llarp::LogInfo("lookup for ", itr->first, " timed out");
            itr = m_PendingRouters.erase(itr);
          }
          else
            ++itr;
        }
      }

      // prefetch addrs
      for(const auto& addr : m_PrefetchAddrs)
      {
        if(!HasPathToService(addr))
        {
          if(!EnsurePathToService(
                 addr, [](Address addr, OutboundContext* ctx) {}, 10000))
          {
            llarp::LogWarn("failed to ensure path to ", addr);
          }
        }
      }

      // prefetch tags
      for(const auto& tag : m_PrefetchTags)
      {
        auto itr = m_PrefetchedTags.find(tag);
        if(itr == m_PrefetchedTags.end())
        {
          itr =
              m_PrefetchedTags.insert(std::make_pair(tag, CachedTagResult(tag)))
                  .first;
        }
        for(const auto& introset : itr->second.result)
        {
          if(HasPendingPathToService(introset.A.Addr()))
            continue;
          if(!EnsurePathToService(introset.A.Addr(),
                                  [](Address addr, OutboundContext* ctx) {},
                                  10000))
          {
            llarp::LogWarn("failed to ensure path to ", introset.A.Addr(),
                           " for tag ", tag.ToString());
          }
        }
        itr->second.Expire(now);
        if(itr->second.ShouldRefresh(now))
        {
          auto path = PickRandomEstablishedPath();
          if(path)
          {
            auto job = new TagLookupJob(this, &itr->second);
            job->SendRequestViaPath(path, Router());
          }
        }
      }

      // tick remote sessions
      {
        auto itr = m_RemoteSessions.begin();
        while(itr != m_RemoteSessions.end())
        {
          if(itr->second->Tick(now))
          {
            m_DeadSessions
                .insert(std::make_pair(itr->first, std::move(itr->second)))
                ->second->markedBad = true;
            itr                     = m_RemoteSessions.erase(itr);
          }
          else
            ++itr;
        }
      }
      // deregister dead sessions
      {
        auto itr = m_DeadSessions.begin();
        while(itr != m_DeadSessions.end())
        {
          if(itr->second->IsDone(now))
            itr = m_DeadSessions.erase(itr);
          else
            ++itr;
        }
      }
    }

    bool
    Endpoint::OutboundContext::IsDone(llarp_time_t now) const
    {
      return now - lastGoodSend > DEFAULT_PATH_LIFETIME;
    }

    uint64_t
    Endpoint::GenTXID()
    {
      uint64_t txid = llarp_randint();
      while(m_PendingLookups.find(txid) != m_PendingLookups.end())
        ++txid;
      return txid;
    }

    std::string
    Endpoint::Name() const
    {
      return m_Name + ":" + m_Identity.pub.Name();
    }

    bool
    Endpoint::HasPathToService(const Address& addr) const
    {
      return m_RemoteSessions.find(addr) != m_RemoteSessions.end();
    }

    void
    Endpoint::PutLookup(IServiceLookup* lookup, uint64_t txid)
    {
      // std::unique_ptr< service::IServiceLookup > ptr(lookup);
      // m_PendingLookups.insert(std::make_pair(txid, ptr));
      // m_PendingLookups[txid] = std::move(ptr);
      m_PendingLookups.insert(
          std::make_pair(txid, std::unique_ptr< IServiceLookup >(lookup)));
    }

    bool
    Endpoint::HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg)
    {
      auto crypto = &m_Router->crypto;
      std::set< IntroSet > remote;
      for(const auto& introset : msg->I)
      {
        if(!introset.Verify(crypto))
        {
          if(m_Identity.pub == introset.A && m_CurrentPublishTX == msg->T)
          {
            IntroSetPublishFail();
          }
          else
          {
            auto itr = m_PendingLookups.find(msg->T);
            if(itr == m_PendingLookups.end())
            {
              llarp::LogWarn(
                  "invalid lookup response for hidden service endpoint ",
                  Name(), " txid=", msg->T);
              return true;
            }
            std::unique_ptr< IServiceLookup > lookup = std::move(itr->second);
            m_PendingLookups.erase(itr);
            lookup->HandleResponse({});
            return true;
          }
          return true;
        }
        if(m_Identity.pub == introset.A && m_CurrentPublishTX == msg->T)
        {
          llarp::LogInfo(
              "got introset publish confirmation for hidden service endpoint ",
              Name());
          IntroSetPublished();
          return true;
        }
        else
        {
          remote.insert(introset);
        }
      }
      auto itr = m_PendingLookups.find(msg->T);
      if(itr == m_PendingLookups.end())
      {
        llarp::LogWarn("invalid lookup response for hidden service endpoint ",
                       Name(), " txid=", msg->T);
        return true;
      }
      std::unique_ptr< IServiceLookup > lookup = std::move(itr->second);
      m_PendingLookups.erase(itr);
      lookup->HandleResponse(remote);
      return true;
    }

    void
    Endpoint::PutSenderFor(const ConvoTag& tag, const ServiceInfo& info)
    {
      auto itr = m_Sessions.find(tag);
      if(itr == m_Sessions.end())
      {
        itr = m_Sessions.insert(std::make_pair(tag, Session{})).first;
      }
      itr->second.remote   = info;
      itr->second.lastUsed = llarp_time_now_ms();
    }

    bool
    Endpoint::GetSenderFor(const ConvoTag& tag, ServiceInfo& si) const
    {
      auto itr = m_Sessions.find(tag);
      if(itr == m_Sessions.end())
        return false;
      si = itr->second.remote;
      return true;
    }

    void
    Endpoint::PutIntroFor(const ConvoTag& tag, const Introduction& intro)
    {
      auto itr = m_Sessions.find(tag);
      if(itr == m_Sessions.end())
      {
        itr = m_Sessions.insert(std::make_pair(tag, Session{})).first;
      }
      itr->second.intro    = intro;
      itr->second.lastUsed = llarp_time_now_ms();
    }

    bool
    Endpoint::GetIntroFor(const ConvoTag& tag, Introduction& intro) const
    {
      auto itr = m_Sessions.find(tag);
      if(itr == m_Sessions.end())
        return false;
      intro = itr->second.intro;
      return true;
    }

    bool
    Endpoint::GetConvoTagsForService(const ServiceInfo& info,
                                     std::set< ConvoTag >& tags) const
    {
      bool inserted = false;
      auto itr      = m_Sessions.begin();
      while(itr != m_Sessions.end())
      {
        if(itr->second.remote == info)
        {
          inserted |= tags.insert(itr->first).second;
        }
        ++itr;
      }
      return inserted;
    }

    bool
    Endpoint::GetCachedSessionKeyFor(const ConvoTag& tag,
                                     const byte_t*& secret) const
    {
      auto itr = m_Sessions.find(tag);
      if(itr == m_Sessions.end())
        return false;
      secret = itr->second.sharedKey.data();
      return true;
    }

    void
    Endpoint::PutCachedSessionKeyFor(const ConvoTag& tag, const SharedSecret& k)
    {
      auto itr = m_Sessions.find(tag);
      if(itr == m_Sessions.end())
      {
        itr = m_Sessions.insert(std::make_pair(tag, Session{})).first;
      }
      itr->second.sharedKey = k;
      itr->second.lastUsed  = llarp_time_now_ms();
    }

    bool
    Endpoint::Start()
    {
      auto crypto = &m_Router->crypto;
      if(m_Keyfile.size())
      {
        if(!m_Identity.EnsureKeys(m_Keyfile, crypto))
          return false;
      }
      else
      {
        m_Identity.RegenerateKeys(crypto);
      }
      if(!m_DataHandler)
      {
        m_DataHandler = this;
      }
      // this does network isolation
      while(m_OnInit.size())
      {
        if(m_OnInit.front()())
          m_OnInit.pop_front();
        else
          return false;
      }
      return true;
    }

    Endpoint::~Endpoint()
    {
    }

    bool
    Endpoint::CachedTagResult::HandleResponse(
        const std::set< IntroSet >& introsets)
    {
      auto now = llarp_time_now_ms();

      for(const auto& introset : introsets)
        if(result.insert(introset).second)
          lastModified = now;
      llarp::LogInfo("Tag result for ", tag.ToString(), " got ",
                     introsets.size(), " results from lookup, have ",
                     result.size(), " cached last modified at ", lastModified,
                     " is ", now - lastModified, "ms old");
      return true;
    }

    void
    Endpoint::CachedTagResult::Expire(llarp_time_t now)
    {
      auto itr = result.begin();
      while(itr != result.end())
      {
        if(itr->HasExpiredIntros(now))
        {
          llarp::LogInfo("Removing expired tag Entry ", itr->A.Name());
          itr          = result.erase(itr);
          lastModified = now;
        }
        else
        {
          ++itr;
        }
      }
    }

    llarp::routing::IMessage*
    Endpoint::CachedTagResult::BuildRequestMessage(uint64_t txid)
    {
      llarp::routing::DHTMessage* msg = new llarp::routing::DHTMessage();
      msg->M.emplace_back(new llarp::dht::FindIntroMessage(tag, txid));
      lastRequest = llarp_time_now_ms();
      return msg;
    }

    bool
    Endpoint::PublishIntroSet(llarp_router* r)
    {
      // publish via near router
      RouterID location = m_Identity.pub.Addr().data();
      auto path         = GetEstablishedPathClosestTo(location);
      if(path && PublishIntroSetVia(r, path))
      {
        // publish via far router
        path = GetEstablishedPathClosestTo(~location);
        return path && PublishIntroSetVia(r, path);
      }
      return false;
    }

    struct PublishIntroSetJob : public IServiceLookup
    {
      IntroSet m_IntroSet;
      Endpoint* m_Endpoint;
      PublishIntroSetJob(Endpoint* parent, uint64_t id,
                         const IntroSet& introset)
          : IServiceLookup(parent, id, "PublishIntroSet")
          , m_IntroSet(introset)
          , m_Endpoint(parent)
      {
      }

      llarp::routing::IMessage*
      BuildRequestMessage()
      {
        llarp::routing::DHTMessage* msg = new llarp::routing::DHTMessage();
        msg->M.emplace_back(
            new llarp::dht::PublishIntroMessage(m_IntroSet, txid, 4));
        return msg;
      }

      bool
      HandleResponse(const std::set< IntroSet >& response)
      {
        if(response.size())
          m_Endpoint->IntroSetPublished();
        else
          m_Endpoint->IntroSetPublishFail();

        return true;
      }
    };

    void
    Endpoint::IntroSetPublishFail()
    {
      // TODO: linear backoff
    }

    bool
    Endpoint::PublishIntroSetVia(llarp_router* r, path::Path* path)
    {
      auto job = new PublishIntroSetJob(this, GenTXID(), m_IntroSet);
      if(job->SendRequestViaPath(path, r))
      {
        m_LastPublishAttempt = llarp_time_now_ms();
        return true;
      }
      return false;
    }

    bool
    Endpoint::ShouldPublishDescriptors(llarp_time_t now) const
    {
      if(NumInStatus(llarp::path::ePathEstablished) < 3)
        return false;
      if(m_IntroSet.HasExpiredIntros(now))
        return now - m_LastPublishAttempt >= INTROSET_PUBLISH_RETRY_INTERVAL;
      return now - m_LastPublishAttempt >= INTROSET_PUBLISH_INTERVAL;
    }

    void
    Endpoint::IntroSetPublished()
    {
      m_LastPublish = llarp_time_now_ms();
      llarp::LogInfo(Name(), " IntroSet publish confirmed");
    }

    struct HiddenServiceAddressLookup : public IServiceLookup
    {
      ~HiddenServiceAddressLookup()
      {
      }

      Address remote;
      typedef std::function< bool(const Address&, const IntroSet*,
                                  const RouterID&) >
          HandlerFunc;
      HandlerFunc handle;

      HiddenServiceAddressLookup(Endpoint* p, HandlerFunc h,
                                 const Address& addr, uint64_t tx)
          : IServiceLookup(p, tx, "HSLookup"), remote(addr), handle(h)
      {
      }

      bool
      HandleResponse(const std::set< IntroSet >& results)
      {
        llarp::LogInfo("found ", results.size(), " for ", remote.ToString());
        if(results.size() > 0)
        {
          return handle(remote, &*results.begin(), endpoint);
        }
        return handle(remote, nullptr, endpoint);
      }

      llarp::routing::IMessage*
      BuildRequestMessage()
      {
        llarp::routing::DHTMessage* msg = new llarp::routing::DHTMessage();
        msg->M.emplace_back(new llarp::dht::FindIntroMessage(txid, remote, 5));
        return msg;
      }
    };

    bool
    Endpoint::DoNetworkIsolation(bool failed)
    {
      if(failed)
        return IsolationFailed();
      llarp_ev_loop_alloc(&m_IsolatedNetLoop);
      return SetupNetworking();
    }

    void
    Endpoint::RunIsolatedMainLoop(void* user)
    {
      Endpoint* self = static_cast< Endpoint* >(user);
      llarp_ev_loop_run_single_process(self->m_IsolatedNetLoop,
                                       self->m_IsolatedWorker,
                                       self->m_IsolatedLogic);
    }

    void
    Endpoint::PutNewOutboundContext(const llarp::service::IntroSet& introset)
    {
      Address addr;
      introset.A.CalculateAddress(addr.data());

      if(m_RemoteSessions.count(addr) >= MAX_OUTBOUND_CONTEXT_COUNT)
      {
        auto itr = m_RemoteSessions.find(addr);

        auto i = m_PendingServiceLookups.find(addr);
        if(i != m_PendingServiceLookups.end())
        {
          auto f = i->second;
          m_PendingServiceLookups.erase(i);
          f(addr, itr->second.get());
        }
        return;
      }

      OutboundContext* ctx = new OutboundContext(introset, this);
      m_RemoteSessions.insert(
          std::make_pair(addr, std::unique_ptr< OutboundContext >(ctx)));
      llarp::LogInfo("Created New outbound context for ", addr.ToString());

      // inform pending
      auto itr = m_PendingServiceLookups.find(addr);
      if(itr != m_PendingServiceLookups.end())
      {
        auto f = itr->second;
        m_PendingServiceLookups.erase(itr);
        f(addr, ctx);
      }
    }

    bool
    Endpoint::HandleGotRouterMessage(const llarp::dht::GotRouterMessage* msg)
    {
      bool success = false;
      if(msg->R.size() == 1)
      {
        auto itr = m_PendingRouters.find(msg->R[0].pubkey);
        if(itr == m_PendingRouters.end())
          return false;
        llarp_async_verify_rc* job = new llarp_async_verify_rc;
        job->nodedb                = m_Router->nodedb;
        job->cryptoworker          = m_Router->tp;
        job->diskworker            = m_Router->disk;
        job->logic                 = nullptr;
        job->hook                  = nullptr;
        job->rc                    = msg->R[0];
        llarp_nodedb_async_verify(job);
        return true;
      }
      return success;
    }

    void
    Endpoint::EnsureRouterIsKnown(const RouterID& router)
    {
      if(router.IsZero())
        return;
      RouterContact rc;
      if(!llarp_nodedb_get_rc(m_Router->nodedb, router, rc))
      {
        if(m_PendingRouters.find(router) == m_PendingRouters.end())
        {
          auto path = GetEstablishedPathClosestTo(router);
          routing::DHTMessage msg;
          auto txid = GenTXID();
          msg.M.emplace_back(
              new dht::FindRouterMessage({}, dht::Key_t(router), txid));

          if(path && path->SendRoutingMessage(&msg, m_Router))
          {
            llarp::LogInfo(Name(), " looking up ", router);
            m_PendingRouters.insert(
                std::make_pair(router, RouterLookupJob(this)));
          }
          else
          {
            llarp::LogError("failed to send request for router lookup");
          }
        }
      }
    }

    void
    Endpoint::HandlePathBuilt(path::Path* p)
    {
      p->SetDataHandler(std::bind(&Endpoint::HandleHiddenServiceFrame, this,
                                  std::placeholders::_1,
                                  std::placeholders::_2));
      p->SetDropHandler(std::bind(&Endpoint::HandleDataDrop, this,
                                  std::placeholders::_1, std::placeholders::_2,
                                  std::placeholders::_3));
      p->SetDeadChecker(std::bind(&Endpoint::CheckPathIsDead, this,
                                  std::placeholders::_1,
                                  std::placeholders::_2));
      path::Builder::HandlePathBuilt(p);
    }

    bool
    Endpoint::HandleDataDrop(path::Path* p, const PathID_t& dst, uint64_t seq)
    {
      llarp::LogWarn(Name(), " message ", seq, " dropped by endpoint ",
                     p->Endpoint(), " via ", dst);
      return true;
    }

    bool
    Endpoint::OutboundContext::HandleDataDrop(path::Path* p,
                                              const PathID_t& dst, uint64_t seq)
    {
      // pick another intro
      if(dst == remoteIntro.pathID && remoteIntro.router == p->Endpoint())
      {
        llarp::LogWarn(Name(), " message ", seq, " dropped by endpoint ",
                       p->Endpoint(), " via ", dst);
        if(MarkCurrentIntroBad(llarp_time_now_ms()))
        {
          llarp::LogInfo(Name(), " switched intros to ", remoteIntro.router,
                         " via ", remoteIntro.pathID);
        }
        UpdateIntroSet(true);
      }
      return true;
    }

    bool
    Endpoint::HandleDataMessage(const PathID_t& src, ProtocolMessage* msg)
    {
      msg->sender.UpdateAddr();
      PutIntroFor(msg->tag, msg->introReply);
      EnsureReplyPath(msg->sender);
      return ProcessDataMessage(msg);
    }

    bool
    Endpoint::HandleHiddenServiceFrame(path::Path* p,
                                       const ProtocolFrame* frame)
    {
      return frame->AsyncDecryptAndVerify(EndpointLogic(), Crypto(), p->RXID(),
                                          Worker(), m_Identity, m_DataHandler);
    }

    Endpoint::SendContext::SendContext(const ServiceInfo& ident,
                                       const Introduction& intro, PathSet* send,
                                       Endpoint* ep)
        : remoteIdent(ident)
        , remoteIntro(intro)
        , m_PathSet(send)
        , m_DataHandler(ep)
        , m_Endpoint(ep)
    {
      createdAt = llarp_time_now_ms();
    }

    void
    Endpoint::OutboundContext::HandlePathBuilt(path::Path* p)
    {
      path::Builder::HandlePathBuilt(p);
      /// don't use it if we are marked bad
      if(markedBad)
        return;
      p->SetDataHandler(
          std::bind(&Endpoint::OutboundContext::HandleHiddenServiceFrame, this,
                    std::placeholders::_1, std::placeholders::_2));
      p->SetDropHandler(std::bind(
          &Endpoint::OutboundContext::HandleDataDrop, this,
          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
      p->SetDeadChecker(std::bind(&Endpoint::CheckPathIsDead, m_Endpoint,
                                  std::placeholders::_1,
                                  std::placeholders::_2));
    }

    void
    Endpoint::HandlePathDead(void* user)
    {
      Endpoint* self = static_cast< Endpoint* >(user);
      self->RegenAndPublishIntroSet(llarp_time_now_ms(), true);
    }

    bool
    Endpoint::CheckPathIsDead(path::Path* p, llarp_time_t latency)
    {
      if(latency >= m_MinPathLatency)
      {
      }
      return false;
    }

    bool
    Endpoint::OutboundContext::HandleHiddenServiceFrame(
        path::Path* p, const ProtocolFrame* frame)
    {
      return m_Endpoint->HandleHiddenServiceFrame(p, frame);
    }

    bool
    Endpoint::OnLookup(const Address& addr, const IntroSet* introset,
                       const RouterID& endpoint)
    {
      auto now = llarp_time_now_ms();
      if(introset == nullptr || introset->IsExpired(now))
      {
        llarp::LogError(Name(), " failed to lookup ", addr.ToString(), " from ",
                        endpoint);
        auto itr = m_PendingServiceLookups.find(addr);
        if(itr != m_PendingServiceLookups.end())
        {
          auto func = itr->second;
          m_PendingServiceLookups.erase(itr);
          func(addr, nullptr);
        }
        m_ServiceLookupFails[endpoint] += 1;
        return false;
      }
      PutNewOutboundContext(*introset);
      return true;
    }

    bool
    Endpoint::EnsurePathToService(const Address& remote, PathEnsureHook hook,
                                  llarp_time_t timeoutMS, bool randomPath)
    {
      path::Path* path = nullptr;
      if(randomPath)
        path = PickRandomEstablishedPath();
      else
        path = GetEstablishedPathClosestTo(remote.ToRouter());
      if(!path)
      {
        llarp::LogWarn("No outbound path for lookup yet");
        return false;
      }
      llarp::LogInfo(Name(), " Ensure Path to ", remote.ToString());
      {
        auto itr = m_RemoteSessions.find(remote);
        if(itr != m_RemoteSessions.end())
        {
          hook(itr->first, itr->second.get());
          return true;
        }
      }
      {
        auto itr = m_PendingServiceLookups.find(remote);
        if(itr != m_PendingServiceLookups.end())
        {
          // duplicate
          llarp::LogWarn("duplicate pending service lookup to ",
                         remote.ToString());
          return false;
        }
      }
      m_PendingServiceLookups.insert(std::make_pair(remote, hook));
      {
        RouterID endpoint = path->Endpoint();
        auto itr          = m_ServiceLookupFails.find(endpoint);
        if(itr != m_ServiceLookupFails.end())
        {
          if(itr->second % 2)
          {
            // get far router
            path = GetEstablishedPathClosestTo(~endpoint);
          }
          else
          {
            path = PickRandomEstablishedPath();
          }
        }
      }
      if(!path)
      {
        llarp::LogError("no backup path");
        return false;
      }

      HiddenServiceAddressLookup* job = new HiddenServiceAddressLookup(
          this,
          std::bind(&Endpoint::OnLookup, this, std::placeholders::_1,
                    std::placeholders::_2, std::placeholders::_3),
          remote, GenTXID());

      if(job->SendRequestViaPath(path, Router()))
        return true;
      llarp::LogError("send via path failed");
      return false;
    }

    Endpoint::OutboundContext::OutboundContext(const IntroSet& introset,
                                               Endpoint* parent)
        : path::Builder(parent->m_Router, parent->m_Router->dht, 3, 4)
        , SendContext(introset.A, {}, this, parent)
        , currentIntroSet(introset)

    {
      updatingIntroSet = false;
      for(const auto intro : introset.I)
      {
        if(intro.expiresAt > m_NextIntro.expiresAt)
        {
          m_NextIntro = intro;
          remoteIntro = intro;
        }
      }
    }

    Endpoint::OutboundContext::~OutboundContext()
    {
    }

    void
    Endpoint::OutboundContext::SwapIntros()
    {
      remoteIntro = m_NextIntro;
      // prepare next intro
      auto now = llarp_time_now_ms();
      for(const auto& intro : currentIntroSet.I)
      {
        if(intro.ExpiresSoon(now))
          continue;
        if(m_BadIntros.find(intro) == m_BadIntros.end()
           && remoteIntro.router == intro.router)
        {
          m_NextIntro = intro;
          return;
        }
      }
      for(const auto& intro : currentIntroSet.I)
      {
        if(intro.ExpiresSoon(now))
          continue;
        if(m_BadIntros.find(intro) == m_BadIntros.end())
        {
          m_NextIntro = intro;
          return;
        }
      }
    }

    bool
    Endpoint::OutboundContext::OnIntroSetUpdate(const Address& addr,
                                                const IntroSet* i,
                                                const RouterID& endpoint)
    {
      if(markedBad)
        return true;
      if(i)
      {
        if(currentIntroSet.T >= i->T)
        {
          llarp::LogInfo("introset is old, dropping");
          return true;
        }
        auto now = llarp_time_now_ms();
        if(i->IsExpired(now))
        {
          llarp::LogError("got expired introset from lookup from ", endpoint);
          return true;
        }
        currentIntroSet = *i;
        if(!ShiftIntroduction())
        {
          llarp::LogWarn("failed to pick new intro during introset update");
        }
        if(GetPathByRouter(m_NextIntro.router) == nullptr)
          BuildOneAlignedTo(m_NextIntro.router);
        else
          SwapIntros();
      }
      updatingIntroSet = false;
      return true;
    }

    bool
    Endpoint::OutboundContext::ReadyToSend() const
    {
      return (!remoteIntro.router.IsZero())
          && GetPathByRouter(remoteIntro.router) != nullptr;
    }

    bool
    Endpoint::SendToOrQueue(const Address& remote, llarp_buffer_t data,
                            ProtocolType t)
    {
      // inbound converstation
      {
        auto itr = m_AddressToService.find(remote);
        if(itr != m_AddressToService.end())
        {
          auto now = llarp_time_now_ms();
          routing::PathTransferMessage transfer;
          ProtocolFrame& f = transfer.T;
          path::Path* p    = nullptr;
          std::set< ConvoTag > tags;
          if(!GetConvoTagsForService(itr->second, tags))
          {
            llarp::LogError("no convo tag");
            return false;
          }
          Introduction remoteIntro;
          const byte_t* K = nullptr;
          for(const auto& tag : tags)
          {
            if(tag.IsZero())
              continue;
            if(p == nullptr && GetIntroFor(tag, remoteIntro))
            {
              if(!remoteIntro.ExpiresSoon(now))
                p = GetPathByRouter(remoteIntro.router);
              if(p)
              {
                f.T = tag;
                if(!GetCachedSessionKeyFor(tag, K))
                {
                  llarp::LogError("no cached session key");
                  return false;
                }
              }
            }
          }
          if(p)
          {
            // TODO: check expiration of our end
            ProtocolMessage m(f.T);
            m.proto      = t;
            m.introReply = p->intro;
            m.sender     = m_Identity.pub;
            m.PutBuffer(data);
            f.N.Randomize();
            f.S = GetSeqNoForConvo(f.T);
            f.C.Zero();
            transfer.Y.Randomize();
            transfer.P = remoteIntro.pathID;
            if(!f.EncryptAndSign(&Router()->crypto, m, K, m_Identity))
            {
              llarp::LogError("failed to encrypt and sign");
              return false;
            }
            llarp::LogDebug(Name(), " send ", data.sz, " via ",
                            remoteIntro.router);
            return p->SendRoutingMessage(&transfer, Router());
          }
        }
      }
      // outbound converstation
      if(HasPathToService(remote))
      {
        auto range = m_RemoteSessions.equal_range(remote);
        auto itr   = range.first;
        while(itr != range.second)
        {
          if(itr->second->ReadyToSend())
          {
            itr->second->AsyncEncryptAndSendTo(data, t);
            return true;
          }
          ++itr;
        }
        llarp::LogWarn("No path ready to send yet");
        // all paths are not ready?
        return false;
      }
      // no converstation
      EnsurePathToService(remote, [](Address, OutboundContext*) {}, 5000,
                          false);
      return false;
    }

    bool
    Endpoint::OutboundContext::BuildOneAlignedTo(const RouterID& remote)
    {
      llarp::LogInfo(Name(), " building path to ", remote);
      auto nodedb = m_Endpoint->Router()->nodedb;
      std::vector< RouterContact > hops;
      hops.resize(numHops);
      for(size_t hop = 0; hop < numHops; ++hop)
      {
        if(hop == 0)
        {
          // first hop
          if(router->NumberOfConnectedRouters())
          {
            if(!router->GetRandomConnectedRouter(hops[0]))
              return false;
          }
          else if(!llarp_nodedb_select_random_hop(nodedb, hops[0], hops[0], 0))
            return false;
        }
        else if(hop == numHops - 1)
        {
          // last hop
          if(!llarp_nodedb_get_rc(nodedb, remote, hops[hop]))
            return false;
        }
        // middle hop
        else
        {
          size_t tries = 5;
          do
          {
            llarp_nodedb_select_random_hop(nodedb, hops[hop - 1], hops[hop],
                                           hop);
            --tries;
          } while(m_Endpoint->Router()->routerProfiling.IsBad(hops[hop].pubkey)
                  && tries > 0);
          return tries > 0;
        }
        return false;
      }
      Build(hops);
      return true;
    }

    bool
    Endpoint::OutboundContext::MarkCurrentIntroBad(llarp_time_t now)
    {
      // insert bad intro
      m_BadIntros[remoteIntro] = now;
      // unconditional shift
      bool shiftedRouter = false;
      bool shiftedIntro  = false;
      // try same router
      for(const auto& intro : currentIntroSet.I)
      {
        if(intro.ExpiresSoon(now))
          continue;
        auto itr = m_BadIntros.find(intro);
        if(itr == m_BadIntros.end() && intro.router == m_NextIntro.router)
        {
          shiftedIntro = true;
          m_NextIntro  = intro;
          break;
        }
      }
      if(!shiftedIntro)
      {
        // try any router
        for(const auto& intro : currentIntroSet.I)
        {
          if(intro.ExpiresSoon(now))
            continue;
          auto itr = m_BadIntros.find(intro);
          if(itr == m_BadIntros.end())
          {
            // TODO: this should always be true but idk if it really is
            shiftedRouter = m_NextIntro.router != intro.router;
            shiftedIntro  = true;
            m_NextIntro   = intro;
            break;
          }
        }
      }
      if(shiftedRouter)
      {
        lastShift = now;
        BuildOneAlignedTo(m_NextIntro.router);
      }
      else if(shiftedIntro)
      {
        SwapIntros();
      }
      return shiftedIntro;
    }

    bool
    Endpoint::OutboundContext::ShiftIntroduction()
    {
      bool success = false;
      auto now     = llarp_time_now_ms();
      if(now - lastShift < MIN_SHIFT_INTERVAL)
        return false;
      bool shifted = false;
      // to find a intro on the same router as before
      for(const auto& intro : currentIntroSet.I)
      {
        if(intro.ExpiresSoon(now))
          continue;
        if(m_BadIntros.find(intro) == m_BadIntros.end()
           && remoteIntro.router == intro.router)
        {
          m_NextIntro = intro;
          return true;
        }
      }
      for(const auto& intro : currentIntroSet.I)
      {
        m_Endpoint->EnsureRouterIsKnown(intro.router);
        if(intro.ExpiresSoon(now))
          continue;
        if(m_BadIntros.find(intro) == m_BadIntros.end() && m_NextIntro != intro)
        {
          shifted = intro.router != m_NextIntro.router
              || (now < intro.expiresAt
                  && intro.expiresAt - now
                      > 10 * 1000);  // TODO: hardcoded value
          m_NextIntro = intro;
          success     = true;
          break;
        }
      }
      if(shifted)
      {
        lastShift = now;
        BuildOneAlignedTo(m_NextIntro.router);
      }
      return success;
    }

    void
    Endpoint::SendContext::AsyncEncryptAndSendTo(llarp_buffer_t data,
                                                 ProtocolType protocol)
    {
      auto now = llarp_time_now_ms();
      if(remoteIntro.ExpiresSoon(now))
      {
        if(!MarkCurrentIntroBad(now))
        {
          llarp::LogWarn("no good path yet, your message may drop");
        }
      }
      if(sequenceNo)
      {
        EncryptAndSendTo(data, protocol);
      }
      else
      {
        AsyncGenIntro(data, protocol);
      }
    }

    struct AsyncKeyExchange
    {
      llarp_logic* logic;
      llarp_crypto* crypto;
      SharedSecret sharedKey;
      ServiceInfo remote;
      const Identity& m_LocalIdentity;
      ProtocolMessage msg;
      ProtocolFrame frame;
      Introduction intro;
      const PQPubKey introPubKey;
      Introduction remoteIntro;
      std::function< void(ProtocolFrame&) > hook;
      IDataHandler* handler;
      ConvoTag tag;

      AsyncKeyExchange(llarp_logic* l, llarp_crypto* c, const ServiceInfo& r,
                       const Identity& localident,
                       const PQPubKey& introsetPubKey,
                       const Introduction& remote, IDataHandler* h,
                       const ConvoTag& t)
          : logic(l)
          , crypto(c)
          , remote(r)
          , m_LocalIdentity(localident)
          , introPubKey(introsetPubKey)
          , remoteIntro(remote)
          , handler(h)
          , tag(t)
      {
      }

      static void
      Result(void* user)
      {
        AsyncKeyExchange* self = static_cast< AsyncKeyExchange* >(user);
        // put values
        self->handler->PutCachedSessionKeyFor(self->msg.tag, self->sharedKey);
        self->handler->PutIntroFor(self->msg.tag, self->remoteIntro);
        self->handler->PutSenderFor(self->msg.tag, self->remote);
        self->hook(self->frame);
        delete self;
      }

      /// given protocol message make protocol frame
      static void
      Encrypt(void* user)
      {
        AsyncKeyExchange* self = static_cast< AsyncKeyExchange* >(user);
        // derive ntru session key component
        SharedSecret K;
        self->crypto->pqe_encrypt(self->frame.C, K, self->introPubKey);
        // randomize Nounce
        self->frame.N.Randomize();
        // compure post handshake session key
        byte_t tmp[64];
        // K
        memcpy(tmp, K, 32);
        // PKE (A, B, N)
        if(!self->m_LocalIdentity.KeyExchange(self->crypto->dh_client, tmp + 32,
                                              self->remote, self->frame.N))
          llarp::LogError("failed to derive x25519 shared key component");
        // H (K + PKE(A, B, N))
        self->crypto->shorthash(self->sharedKey,
                                llarp::StackBuffer< decltype(tmp) >(tmp));
        // set tag
        self->msg.tag = self->tag;
        // set sender
        self->msg.sender = self->m_LocalIdentity.pub;
        // set version
        self->msg.version = LLARP_PROTO_VERSION;
        // set protocol
        self->msg.proto = eProtocolTraffic;
        // encrypt and sign
        if(self->frame.EncryptAndSign(self->crypto, self->msg, K,
                                      self->m_LocalIdentity))
          llarp_logic_queue_job(self->logic, {self, &Result});
        else
        {
          llarp::LogError("failed to encrypt and sign");
          delete self;
        }
      }
    };

    void
    Endpoint::EnsureReplyPath(const ServiceInfo& ident)
    {
      m_AddressToService[ident.Addr()] = ident;
    }

    void
    Endpoint::OutboundContext::AsyncGenIntro(llarp_buffer_t payload,
                                             ProtocolType t)
    {
      auto path = m_PathSet->GetPathByRouter(remoteIntro.router);
      if(path == nullptr)
      {
        // try parent as fallback
        path = m_Endpoint->GetPathByRouter(remoteIntro.router);
        if(path == nullptr)
        {
          llarp::LogWarn(Name(), " dropping intro frame, no path to ",
                         remoteIntro.router);
          return;
        }
      }
      currentConvoTag.Randomize();
      AsyncKeyExchange* ex = new AsyncKeyExchange(
          m_Endpoint->RouterLogic(), m_Endpoint->Crypto(), remoteIdent,
          m_Endpoint->GetIdentity(), currentIntroSet.K, remoteIntro,
          m_DataHandler, currentConvoTag);

      ex->hook = std::bind(&Endpoint::OutboundContext::Send, this,
                           std::placeholders::_1);

      ex->msg.PutBuffer(payload);
      ex->msg.introReply = path->intro;
      llarp_threadpool_queue_job(m_Endpoint->Worker(),
                                 {ex, &AsyncKeyExchange::Encrypt});
    }

    void
    Endpoint::SendContext::Send(ProtocolFrame& msg)
    {
      auto path = m_PathSet->GetPathByRouter(remoteIntro.router);
      if(path == nullptr)
        path = m_Endpoint->GetPathByRouter(remoteIntro.router);
      if(path)
      {
        ++sequenceNo;
        routing::PathTransferMessage transfer(msg, remoteIntro.pathID);
        if(path->SendRoutingMessage(&transfer, m_Endpoint->Router()))
        {
          llarp::LogDebug("sent data to ", remoteIntro.pathID, " on ",
                          remoteIntro.router);
          lastGoodSend = llarp_time_now_ms();
        }
        else
          llarp::LogError("Failed to send frame on path");
      }
      else
        llarp::LogError("cannot send becuase we have no path to ",
                        remoteIntro.router);
    }

    std::string
    Endpoint::OutboundContext::Name() const
    {
      return "OBContext:" + m_Endpoint->Name() + "-"
          + currentIntroSet.A.Addr().ToString();
    }

    void
    Endpoint::OutboundContext::UpdateIntroSet(bool randomizePath)
    {
      if(updatingIntroSet || markedBad)
        return;
      auto addr = currentIntroSet.A.Addr();

      path::Path* path = nullptr;
      if(randomizePath)
        path = m_Endpoint->PickRandomEstablishedPath();
      else
        path = m_Endpoint->GetEstablishedPathClosestTo(addr.data());

      if(path)
      {
        HiddenServiceAddressLookup* job = new HiddenServiceAddressLookup(
            m_Endpoint,
            std::bind(&Endpoint::OutboundContext::OnIntroSetUpdate, this,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3),
            addr, m_Endpoint->GenTXID());

        updatingIntroSet = job->SendRequestViaPath(path, m_Endpoint->Router());
      }
      else
      {
        llarp::LogWarn(
            "Cannot update introset no path for outbound session to ",
            currentIntroSet.A.Addr().ToString());
      }
    }

    bool
    Endpoint::OutboundContext::Tick(llarp_time_t now)
    {
      if(remoteIntro.ExpiresSoon(now))
      {
        // shift intro if it expires "soon"
        ShiftIntroduction();

        if(remoteIntro != m_NextIntro)
        {
          if(GetPathByRouter(m_NextIntro.router) != nullptr)
          {
            // we can safely set remoteIntro to the next one
            SwapIntros();
            llarp::LogInfo(Name(), "swapped intro");
          }
        }
      }
      // lookup router in intro if set and unknown
      if(!remoteIntro.router.IsZero())
        m_Endpoint->EnsureRouterIsKnown(remoteIntro.router);
      // expire bad intros
      auto itr = m_BadIntros.begin();
      while(itr != m_BadIntros.end())
      {
        if(now - itr->second > DEFAULT_PATH_LIFETIME)
          itr = m_BadIntros.erase(itr);
        else
          ++itr;
      }
      // if we are dead return true so we are removed
      return lastGoodSend
          ? (now >= lastGoodSend && now - lastGoodSend > sendTimeout)
          : (now >= createdAt && now - createdAt > connectTimeout);
    }

    bool
    Endpoint::OutboundContext::SelectHop(llarp_nodedb* db,
                                         const RouterContact& prev,
                                         RouterContact& cur, size_t hop)
    {
      if(m_NextIntro.router.IsZero())
        return false;
      if(hop == numHops - 1)
      {
        if(llarp_nodedb_get_rc(db, m_NextIntro.router, cur))
        {
          return true;
        }
        else
        {
          // we don't have it?
          llarp::LogError(
              "cannot build aligned path, don't have router for "
              "introduction ",
              m_NextIntro);
          m_Endpoint->EnsureRouterIsKnown(m_NextIntro.router);
          return false;
        }
      }
      return path::Builder::SelectHop(db, prev, cur, hop);
    }

    uint64_t
    Endpoint::GetSeqNoForConvo(const ConvoTag& tag)
    {
      auto itr = m_Sessions.find(tag);
      if(itr == m_Sessions.end())
        return 0;
      return ++(itr->second.seqno);
    }

    bool
    Endpoint::OutboundContext::ShouldBuildMore() const
    {
      if(markedBad)
        return false;
      bool should = path::Builder::ShouldBuildMore();
      // determinte newest intro
      Introduction intro;
      if(!GetNewestIntro(intro))
        return should;

      auto now = llarp_time_now_ms();
      // time from now that the newest intro expires at
      if(now >= intro.expiresAt)
        return should;
      auto dlt = now - intro.expiresAt;
      return should
          || (  // try spacing tunnel builds out evenly in time
                 (dlt < (DEFAULT_PATH_LIFETIME / 2))
                 && (NumInStatus(path::ePathBuilding) < m_NumPaths)
                 && (dlt > buildIntervalLimit));
    }

    /// send on an established convo tag
    void
    Endpoint::SendContext::EncryptAndSendTo(llarp_buffer_t payload,
                                            ProtocolType t)
    {
      auto crypto          = m_Endpoint->Router()->crypto;
      const byte_t* shared = nullptr;
      routing::PathTransferMessage msg;
      ProtocolFrame& f = msg.T;
      f.N.Randomize();
      f.T = currentConvoTag;
      f.S = m_Endpoint->GetSeqNoForConvo(f.T);

      auto now = llarp_time_now_ms();
      if(remoteIntro.ExpiresSoon(now))
      {
        // shift intro
        MarkCurrentIntroBad(now);
      }

      auto path = m_PathSet->GetNewestPathByRouter(remoteIntro.router);
      if(!path)
      {
        path = m_Endpoint->GetNewestPathByRouter(remoteIntro.router);
        if(!path)
        {
          llarp::LogError("cannot encrypt and send: no path for intro ",
                          remoteIntro);
          return;
        }
      }

      if(m_DataHandler->GetCachedSessionKeyFor(f.T, shared))
      {
        ProtocolMessage m;
        m.proto = t;
        m_DataHandler->PutIntroFor(f.T, remoteIntro);
        m.introReply = path->intro;
        m.sender     = m_Endpoint->m_Identity.pub;
        m.PutBuffer(payload);
        m.tag = f.T;

        if(!f.EncryptAndSign(&crypto, m, shared, m_Endpoint->m_Identity))
        {
          llarp::LogError("failed to sign");
          return;
        }
      }
      else
      {
        llarp::LogError("No cached session key");
        return;
      }

      msg.P = remoteIntro.pathID;
      msg.Y.Randomize();
      ++sequenceNo;
      if(path->SendRoutingMessage(&msg, m_Endpoint->Router()))
      {
        llarp::LogDebug("sent message via ", remoteIntro.pathID, " on ",
                        remoteIntro.router);
        lastGoodSend = now;
      }
      else
      {
        llarp::LogWarn("Failed to send routing message for data");
      }
    }

    llarp_logic*
    Endpoint::RouterLogic()
    {
      return m_Router->logic;
    }

    llarp_logic*
    Endpoint::EndpointLogic()
    {
      return m_IsolatedLogic ? m_IsolatedLogic : m_Router->logic;
    }

    llarp_crypto*
    Endpoint::Crypto()
    {
      return &m_Router->crypto;
    }

    llarp_threadpool*
    Endpoint::Worker()
    {
      return m_Router->tp;
    }

  }  // namespace service
}  // namespace llarp
