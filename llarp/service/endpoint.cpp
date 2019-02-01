#include <service/endpoint.hpp>

#include <dht/messages/findintro.hpp>
#include <dht/messages/findrouter.hpp>
#include <dht/messages/gotintro.hpp>
#include <dht/messages/gotrouter.hpp>
#include <dht/messages/pubintro.hpp>
#include <messages/dht.hpp>
#include <router/router.hpp>
#include <service/protocol.hpp>
#include <util/logic.hpp>

#include <util/buffer.hpp>

namespace llarp
{
  namespace service
  {
    Endpoint::Endpoint(const std::string& name, llarp::Router* r,
                       Context* parent)
        : path::Builder(r, r->dht, 6, DEFAULT_HOP_LENGTH)
        , context(parent)
        , m_Router(r)
        , m_Name(name)
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
      return false;
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
        if(ShouldBuildMore(now) || forceRebuild)
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
      if(!m_Identity.SignIntroSet(m_IntroSet, m_Router->crypto.get(), now))
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
    Endpoint::FlushSNodeTraffic()
    {
      auto itr = m_SNodeSessions.begin();
      while(itr != m_SNodeSessions.end())
      {
        itr->second->Flush();
        ++itr;
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
      // expire snode sessions
      {
        auto itr = m_SNodeSessions.begin();
        while(itr != m_SNodeSessions.end())
        {
          if(itr->second->IsExpired(now))
            itr = m_SNodeSessions.erase(itr);
          else
            ++itr;
        }
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
                 addr,
                 [](__attribute__((unused)) Address addr,
                    __attribute__((unused)) OutboundContext* ctx) {},
                 10000))
          {
            llarp::LogWarn("failed to ensure path to ", addr);
          }
        }
      }
#ifdef TESTNET
      // prefetch tags
      for(const auto& tag : m_PrefetchTags)
      {
        auto itr = m_PrefetchedTags.find(tag);
        if(itr == m_PrefetchedTags.end())
        {
          itr = m_PrefetchedTags
                    .insert(std::make_pair(tag, CachedTagResult(tag, this)))
                    .first;
        }
        for(const auto& introset : itr->second.result)
        {
          if(HasPendingPathToService(introset.A.Addr()))
            continue;
          byte_t tmp[1024] = {0};
          auto buf         = StackBuffer< decltype(tmp) >(tmp);
          if(!SendToServiceOrQueue(introset.A.Addr().data().data(), buf,
                                   eProtocolText))
          {
            llarp::LogWarn(Name(), " failed to send/queue data to ",
                           introset.A.Addr(), " for tag ", tag.ToString());
          }
        }
        itr->second.Expire(now);
        if(itr->second.ShouldRefresh(now))
        {
          auto path = PickRandomEstablishedPath();
          if(path)
          {
            auto job = new TagLookupJob(this, &itr->second);
            if(!job->SendRequestViaPath(path, Router()))
              llarp::LogError(Name(), " failed to send tag lookup");
          }
          else
          {
            llarp::LogError(Name(), " has no paths for tag lookup");
          }
        }
      }
#endif

      // tick remote sessions
      {
        auto itr = m_RemoteSessions.begin();
        while(itr != m_RemoteSessions.end())
        {
          if(itr->second->Tick(now))
          {
            itr->second->Stop();
            m_DeadSessions.insert(
                std::make_pair(itr->first, std::move(itr->second)));
            itr = m_RemoteSessions.erase(itr);
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
    Endpoint::OutboundContext::Stop()
    {
      markedBad = true;
      return llarp::path::Builder::Stop();
    }

    bool
    Endpoint::Stop()
    {
      // stop remote sessions
      for(auto& item : m_RemoteSessions)
      {
        item.second->Stop();
      }
      // stop snode sessions
      for(auto& item : m_SNodeSessions)
      {
        item.second->Stop();
      }
      return llarp::path::Builder::Stop();
    }

    bool
    Endpoint::OutboundContext::IsDone(llarp_time_t now) const
    {
      return now - lastGoodSend > DEFAULT_PATH_LIFETIME && ShouldRemove();
    }

    uint64_t
    Endpoint::GenTXID()
    {
      uint64_t txid = llarp::randint();
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
      auto range                   = m_RemoteSessions.equal_range(addr);
      Sessions::const_iterator itr = range.first;
      while(itr != range.second)
      {
        if(itr->second->ReadyToSend())
          return true;
        ++itr;
      }
      return false;
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
      auto crypto = m_Router->crypto.get();
      std::set< IntroSet > remote;
      for(const auto& introset : msg->I)
      {
        if(!introset.Verify(crypto, Now()))
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
      itr->second.lastUsed = Now();
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
      itr->second.lastUsed = Now();
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
                                     SharedSecret& secret) const
    {
      auto itr = m_Sessions.find(tag);
      if(itr == m_Sessions.end())
        return false;
      secret = itr->second.sharedKey;
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
      itr->second.lastUsed  = Now();
    }

    bool
    Endpoint::LoadKeyFile()
    {
      auto crypto = m_Router->crypto.get();
      if(m_Keyfile.size())
      {
        if(!m_Identity.EnsureKeys(m_Keyfile, crypto))
        {
          llarp::LogWarn("Can't ensure keyfile [", m_Keyfile, "]");
          return false;
        }
      }
      else
      {
        m_Identity.RegenerateKeys(crypto);
      }
      return true;
    }

    bool
    Endpoint::Start()
    {
      // how can I tell if a m_Identity isn't loaded?
      // this->LoadKeyFile();
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
        {
          llarp::LogWarn("Can't call init of network isolation");
          return false;
        }
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
      auto now = parent->Now();

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
      lastRequest = parent->Now();
      return msg;
    }

    bool
    Endpoint::PublishIntroSet(llarp::Router* r)
    {
      // publish via near router
      RouterID location = m_Identity.pub.Addr().as_array();
      auto path         = GetEstablishedPathClosestTo(location);
      return path && PublishIntroSetVia(r, path);
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
            new llarp::dht::PublishIntroMessage(m_IntroSet, txid, 1));
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
    Endpoint::PublishIntroSetVia(llarp::Router* r, path::Path* path)
    {
      auto job = new PublishIntroSetJob(this, GenTXID(), m_IntroSet);
      if(job->SendRequestViaPath(path, r))
      {
        m_LastPublishAttempt = Now();
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
      m_LastPublish = Now();
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
        msg->M.emplace_back(new llarp::dht::FindIntroMessage(txid, remote, 0));
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
      introset.A.CalculateAddress(addr.as_array());

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
        job->logic                 = m_Router->logic;
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
      if(!m_Router->nodedb->Get(router, rc))
      {
        LookupRouterAnon(router);
      }
    }

    bool
    Endpoint::LookupRouterAnon(RouterID router)
    {
      if(m_PendingRouters.find(router) == m_PendingRouters.end())
      {
        auto path = GetEstablishedPathClosestTo(router);
        routing::DHTMessage msg;
        auto txid = GenTXID();
        msg.M.emplace_back(new dht::FindRouterMessage(txid, router));

        if(path && path->SendRoutingMessage(&msg, m_Router))
        {
          llarp::LogInfo(Name(), " looking up ", router);
          m_PendingRouters.insert(
              std::make_pair(router, RouterLookupJob(this)));
          return true;
        }
        else
          llarp::LogError("failed to send request for router lookup");
      }
      return false;
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
        if(MarkCurrentIntroBad(Now()))
        {
          llarp::LogInfo(Name(), " switched intros to ", remoteIntro.router,
                         " via ", remoteIntro.pathID);
        }
        UpdateIntroSet(true);
      }
      return true;
    }

    bool
    Endpoint::HandleDataMessage(__attribute__((unused)) const PathID_t& src,
                                ProtocolMessage* msg)
    {
      msg->sender.UpdateAddr();
      PutIntroFor(msg->tag, msg->introReply);
      EnsureReplyPath(msg->sender);
      return ProcessDataMessage(msg);
    }

    bool
    Endpoint::HasPathToSNode(const llarp::RouterID& ident) const
    {
      auto range = m_SNodeSessions.equal_range(ident);
      auto itr   = range.first;
      while(itr != range.second)
      {
        if(itr->second->IsReady())
        {
          return true;
        }
        ++itr;
      }
      return false;
    }

    bool
    Endpoint::ProcessDataMessage(ProtocolMessage* msg)
    {
      if(msg->proto == eProtocolTraffic)
      {
        auto buf = llarp::Buffer(msg->payload);
        return HandleWriteIPPacket(buf,
                                   std::bind(&Endpoint::ObtainIPForAddr, this,
                                             msg->sender.Addr(), false));
      }
      else if(msg->proto == eProtocolText)
      {
        // TODO: implement me (?)
        return true;
      }
      return false;
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
      createdAt = ep->Now();
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
      self->RegenAndPublishIntroSet(self->Now(), true);
    }

    bool
    Endpoint::CheckPathIsDead(__attribute__((unused)) path::Path* p,
                              __attribute__((unused)) llarp_time_t latency)
    {
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
      auto now = Now();
      if(introset == nullptr || introset->IsExpired(now))
      {
        llarp::LogError(Name(), " failed to lookup ", addr.ToString(), " from ",
                        endpoint);
        m_ServiceLookupFails[endpoint] = m_ServiceLookupFails[endpoint] + 1;
        auto itr                       = m_PendingServiceLookups.find(addr);
        if(itr != m_PendingServiceLookups.end())
        {
          auto func = itr->second;
          m_PendingServiceLookups.erase(itr);
          func(addr, nullptr);
        }
        return false;
      }
      PutNewOutboundContext(*introset);
      return true;
    }

    bool
    Endpoint::EnsurePathToService(const Address& remote, PathEnsureHook hook,
                                  __attribute__((unused))
                                  llarp_time_t timeoutMS,
                                  bool randomPath)
    {
      path::Path* path = nullptr;
      if(randomPath)
        path = PickRandomEstablishedPath();
      else
        path = GetEstablishedPathClosestTo(remote.ToRouter());
      if(!path)
      {
        llarp::LogWarn("No outbound path for lookup yet");
        BuildOne();
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
          path = PickRandomEstablishedPath();
        }
      }
      if(!path)
      {
        path = PickRandomEstablishedPath();
        if(!path)
        {
          llarp::LogError(Name(), "no working paths for lookup");
          hook(remote, nullptr);
          return false;
        }
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
        : path::Builder(parent->m_Router, parent->m_Router->dht, 3,
                        DEFAULT_HOP_LENGTH)
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

    /// actually swap intros
    void
    Endpoint::OutboundContext::SwapIntros()
    {
      remoteIntro = m_NextIntro;
      // prepare next intro
      auto now = Now();
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
    Endpoint::OutboundContext::OnIntroSetUpdate(__attribute__((unused))
                                                const Address& addr,
                                                const IntroSet* i,
                                                const RouterID& endpoint)
    {
      if(markedBad)
        return true;
      updatingIntroSet = false;
      if(i)
      {
        if(currentIntroSet.T >= i->T)
        {
          llarp::LogInfo("introset is old, dropping");
          return true;
        }
        auto now = Now();
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
      return true;
    }

    bool
    Endpoint::OutboundContext::ReadyToSend() const
    {
      return (!remoteIntro.router.IsZero())
          && GetPathByRouter(remoteIntro.router) != nullptr;
    }

    void
    Endpoint::EnsurePathToSNode(const RouterID& snode)
    {
      if(m_SNodeSessions.count(snode) == 0)
      {
        auto themIP = ObtainIPForAddr(snode, true);
        m_SNodeSessions.emplace(std::make_pair(
            snode,
            std::unique_ptr< llarp::exit::BaseSession >(
                new llarp::exit::SNodeSession(
                    snode,
                    std::bind(&Endpoint::HandleWriteIPPacket, this,
                              std::placeholders::_1,
                              [themIP]() -> huint32_t { return themIP; }),
                    m_Router, 2, numHops))));
      }
    }

    bool
    Endpoint::SendToSNodeOrQueue(const RouterID& addr, const llarp_buffer_t& buf)
    {
      llarp::net::IPv4Packet pkt;
      if(!pkt.Load(buf))
        return false;
      auto range = m_SNodeSessions.equal_range(addr);
      auto itr   = range.first;
      while(itr != range.second)
      {
        if(itr->second->IsReady())
        {
          if(itr->second->QueueUpstreamTraffic(pkt,
                                               llarp::routing::ExitPadSize))
          {
            return true;
          }
        }
        ++itr;
      }
      return false;
    }

    bool
    Endpoint::SendToServiceOrQueue(const RouterID& addr,
                                   const llarp_buffer_t& data, ProtocolType t)
    {
      service::Address remote(addr.as_array());

      // inbound converstation
      auto now = Now();

      {
        auto itr = m_AddressToService.find(remote);
        if(itr != m_AddressToService.end())
        {
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
          SharedSecret K;
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
            if(!f.EncryptAndSign(Router()->crypto.get(), m, K, m_Identity))
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
      }
      // no converstation
      return EnsurePathToService(
          remote,
          [](Address, OutboundContext* c) {
            if(c)
              c->UpdateIntroSet(true);
          },
          5000, false);
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
          else
            return false;
        }
        else if(hop == numHops - 1)
        {
          // last hop
          if(!nodedb->Get(remote, hops[hop]))
            return false;
        }
        // middle hop
        else
        {
          size_t tries = 5;
          do
          {
            nodedb->select_random_hop(hops[hop - 1], hops[hop], hop);
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
      else
      {
        llarp::LogInfo(Name(), " updating introset");
        UpdateIntroSet(false);
      }
      return shiftedIntro;
    }

    bool
    Endpoint::OutboundContext::ShiftIntroduction()
    {
      bool success = false;
      auto now     = Now();
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
    Endpoint::SendContext::AsyncEncryptAndSendTo(const llarp_buffer_t& data,
                                                 ProtocolType protocol)
    {
      auto now = m_Endpoint->Now();
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
      llarp::Logic* logic;
      llarp::Crypto* crypto;
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

      AsyncKeyExchange(llarp::Logic* l, llarp::Crypto* c, const ServiceInfo& r,
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
        // randomize Nonce
        self->frame.N.Randomize();
        // compure post handshake session key
        // PKE (A, B, N)
        SharedSecret sharedSecret;
        using namespace std::placeholders;
        path_dh_func dh_client =
            std::bind(&Crypto::dh_client, self->crypto, _1, _2, _3, _4);
        if(!self->m_LocalIdentity.KeyExchange(dh_client, sharedSecret,
                                              self->remote, self->frame.N))
        {
          llarp::LogError("failed to derive x25519 shared key component");
        }
        std::array< byte_t, 64 > tmp;
        // K
        std::copy(K.begin(), K.end(), tmp.begin());
        // H (K + PKE(A, B, N))
        std::copy(sharedSecret.begin(), sharedSecret.end(), tmp.begin() + 32);
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
          self->logic->queue_job({self, &Result});
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
    Endpoint::OutboundContext::AsyncGenIntro(const llarp_buffer_t& payload,
                                             __attribute__((unused))
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
          lastGoodSend = m_Endpoint->Now();
        }
        else
          llarp::LogError("Failed to send frame on path");
      }
      else
        llarp::LogError("cannot send because we have no path to ",
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
        path = m_Endpoint->GetEstablishedPathClosestTo(addr.as_array());

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
      // check for expiration
      if(remoteIntro.ExpiresSoon(now))
      {
        // shift intro if it expires "soon"
        ShiftIntroduction();
      }
      // swap if we can
      if(remoteIntro != m_NextIntro)
      {
        if(GetPathByRouter(m_NextIntro.router) != nullptr)
        {
          // we can safely set remoteIntro to the next one
          SwapIntros();
          llarp::LogInfo(Name(), "swapped intro");
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
                                         RouterContact& cur, size_t hop,
                                         llarp::path::PathRole roles)
    {
      if(m_NextIntro.router.IsZero())
        return false;
      if(hop == numHops - 1)
      {
        if(db->Get(m_NextIntro.router, cur))
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
      (void)roles;
      return path::Builder::SelectHop(db, prev, cur, hop, roles);
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
    Endpoint::OutboundContext::ShouldBuildMore(llarp_time_t now) const
    {
      if(markedBad)
        return false;
      bool should = path::Builder::ShouldBuildMore(now);
      // determine newest intro
      Introduction intro;
      if(!GetNewestIntro(intro))
        return should;
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
    Endpoint::SendContext::EncryptAndSendTo(const llarp_buffer_t& payload,
                                            ProtocolType t)
    {
      auto crypto = m_Endpoint->Router()->crypto.get();
      SharedSecret shared;
      routing::PathTransferMessage msg;
      ProtocolFrame& f = msg.T;
      f.N.Randomize();
      f.T = currentConvoTag;
      f.S = m_Endpoint->GetSeqNoForConvo(f.T);

      auto now = m_Endpoint->Now();
      if(remoteIntro.ExpiresSoon(now))
      {
        // shift intro
        if(MarkCurrentIntroBad(now))
        {
          llarp::LogInfo("intro shifted");
        }
      }
      auto path = m_PathSet->GetNewestPathByRouter(remoteIntro.router);
      if(!path)
      {
        llarp::LogError("cannot encrypt and send: no path for intro ",
                        remoteIntro);

        return;
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

        if(!f.EncryptAndSign(crypto, m, shared, m_Endpoint->m_Identity))
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

    llarp::Logic*
    Endpoint::RouterLogic()
    {
      return m_Router->logic;
    }

    llarp::Logic*
    Endpoint::EndpointLogic()
    {
      return m_IsolatedLogic ? m_IsolatedLogic : m_Router->logic;
    }

    llarp::Crypto*
    Endpoint::Crypto()
    {
      return m_Router->crypto.get();
    }

    llarp_threadpool*
    Endpoint::Worker()
    {
      return m_Router->tp;
    }

  }  // namespace service
}  // namespace llarp
