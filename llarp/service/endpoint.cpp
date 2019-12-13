#include <service/endpoint.hpp>

#include <dht/messages/findintro.hpp>
#include <dht/messages/findrouter.hpp>
#include <dht/messages/gotintro.hpp>
#include <dht/messages/gotrouter.hpp>
#include <dht/messages/pubintro.hpp>
#include <nodedb.hpp>
#include <profiling.hpp>
#include <router/abstractrouter.hpp>
#include <routing/dht_message.hpp>
#include <routing/path_transfer_message.hpp>
#include <service/endpoint_state.hpp>
#include <service/endpoint_util.hpp>
#include <service/hidden_service_address_lookup.hpp>
#include <service/outbound_context.hpp>
#include <service/protocol.hpp>
#include <util/thread/logic.hpp>
#include <util/str.hpp>
#include <util/buffer.hpp>
#include <util/meta/memfn.hpp>
#include <hook/shell.hpp>

#include <utility>

namespace llarp
{
  namespace service
  {
    Endpoint::Endpoint(const std::string& name, AbstractRouter* r,
                       Context* parent)
        : path::Builder(r, 3, path::default_len)
        , context(parent)
        , m_RecvQueue(128)
    {
      m_state           = std::make_unique< EndpointState >();
      m_state->m_Router = r;
      m_state->m_Name   = name;
      m_state->m_Tag.Zero();
      m_RecvQueue.enable();
    }

    bool
    Endpoint::SetOption(const std::string& k, const std::string& v)
    {
      return m_state->SetOption(k, v, *this);
    }

    llarp_ev_loop_ptr
    Endpoint::EndpointNetLoop()
    {
      if(m_state->m_IsolatedNetLoop)
        return m_state->m_IsolatedNetLoop;

      return Router()->netloop();
    }

    bool
    Endpoint::NetworkIsIsolated() const
    {
      return m_state->m_IsolatedLogic.get() != nullptr
          && m_state->m_IsolatedNetLoop != nullptr;
    }

    bool
    Endpoint::HasPendingPathToService(const Address& addr) const
    {
      return m_state->m_PendingServiceLookups.find(addr)
          != m_state->m_PendingServiceLookups.end();
    }

    void
    Endpoint::RegenAndPublishIntroSet(bool forceRebuild)
    {
      const auto now = llarp::time_now_ms();
      std::set< Introduction > I;
      if(!GetCurrentIntroductionsWithFilter(
             I, [now](const service::Introduction& intro) -> bool {
               return not intro.ExpiresSoon(now, 2 * 60 * 1000);
             }))
      {
        LogWarn("could not publish descriptors for endpoint ", Name(),
                " because we couldn't get enough valid introductions");
        if(ShouldBuildMore(now) || forceRebuild)
          ManualRebuild(1);
        return;
      }
      introSet().I.clear();
      for(auto& intro : I)
      {
        introSet().I.emplace_back(std::move(intro));
      }
      if(introSet().I.size() == 0)
      {
        LogWarn("not enough intros to publish introset for ", Name());
        if(ShouldBuildMore(now) || forceRebuild)
          ManualRebuild(1);
        return;
      }
      introSet().topic = m_state->m_Tag;
      if(!m_Identity.SignIntroSet(introSet(), now))
      {
        LogWarn("failed to sign introset for endpoint ", Name());
        return;
      }
      if(PublishIntroSet(Router()))
      {
        LogInfo("(re)publishing introset for endpoint ", Name());
      }
      else
      {
        LogWarn("failed to publish intro set for endpoint ", Name());
      }
    }

    bool
    Endpoint::IsReady() const
    {
      const auto now = Now();
      if(introSet().I.size() == 0)
        return false;
      if(introSet().IsExpired(now))
        return false;
      return true;
    }

    bool
    Endpoint::HasPendingRouterLookup(const RouterID remote) const
    {
      const auto& routers = m_state->m_PendingRouters;
      return routers.find(remote) != routers.end();
    }

    bool
    Endpoint::GetEndpointWithConvoTag(const ConvoTag tag,
                                      llarp::AlignedBuffer< 32 >& addr,
                                      bool& snode) const
    {
      auto itr = Sessions().find(tag);
      if(itr != Sessions().end())
      {
        snode = false;
        addr  = itr->second.remote.Addr();
        return true;
      }

      for(const auto& item : m_state->m_SNodeSessions)
      {
        if(item.second.second == tag)
        {
          snode = true;
          addr  = item.first;
          return true;
        }
      }

      return false;
    }

    bool
    Endpoint::IntrosetIsStale() const
    {
      return introSet().HasExpiredIntros(Now());
    }

    util::StatusObject
    Endpoint::ExtractStatus() const
    {
      auto obj        = path::Builder::ExtractStatus();
      obj["identity"] = m_Identity.pub.Addr().ToString();
      return m_state->ExtractStatus(obj);
    }

    void Endpoint::Tick(llarp_time_t)
    {
      const auto now = llarp::time_now_ms();
      path::Builder::Tick(now);
      // publish descriptors
      if(ShouldPublishDescriptors(now))
      {
        RegenAndPublishIntroSet();
      }

      // expire snode sessions
      EndpointUtil::ExpireSNodeSessions(now, m_state->m_SNodeSessions);
      // expire pending tx
      EndpointUtil::ExpirePendingTx(now, m_state->m_PendingLookups);
      // expire pending router lookups
      EndpointUtil::ExpirePendingRouterLookups(now, m_state->m_PendingRouters);

      // prefetch addrs
      for(const auto& addr : m_state->m_PrefetchAddrs)
      {
        if(!EndpointUtil::HasPathToService(addr, m_state->m_RemoteSessions))
        {
          if(!EnsurePathToService(
                 addr,
                 [](ABSL_ATTRIBUTE_UNUSED Address _addr,
                    ABSL_ATTRIBUTE_UNUSED OutboundContext* _ctx) {},
                 10000))
          {
            LogWarn("failed to ensure path to ", addr);
          }
        }
      }
#ifdef TESTNET
      // prefetch tags
      for(const auto& tag : m_state->m_PrefetchTags)
      {
        auto itr = m_state->m_PrefetchedTags.find(tag);
        if(itr == m_state->m_PrefetchedTags.end())
        {
          itr =
              m_state->m_PrefetchedTags.emplace(tag, CachedTagResult(tag, this))
                  .first;
        }
        for(const auto& introset : itr->second.result)
        {
          if(HasPendingPathToService(introset.A.Addr()))
            continue;
          std::array< byte_t, 128 > tmp = {0};
          llarp_buffer_t buf(tmp);
          if(SendToServiceOrQueue(introset.A.Addr(), buf, eProtocolControl))
            LogInfo(Name(), " send message to ", introset.A.Addr(), " for tag ",
                    tag.ToString());
          else

            LogWarn(Name(), " failed to send/queue data to ", introset.A.Addr(),
                    " for tag ", tag.ToString());
        }
        itr->second.Expire(now);
        if(itr->second.ShouldRefresh(now))
        {
          auto path = PickRandomEstablishedPath();
          if(path)
          {
            auto job = new TagLookupJob(this, &itr->second);
            if(!job->SendRequestViaPath(path, Router()))
              LogError(Name(), " failed to send tag lookup");
          }
          else
          {
            LogError(Name(), " has no paths for tag lookup");
          }
        }
      }
#endif

      // deregister dead sessions
      EndpointUtil::DeregisterDeadSessions(now, m_state->m_DeadSessions);
      // tick remote sessions
      EndpointUtil::TickRemoteSessions(now, m_state->m_RemoteSessions,
                                       m_state->m_DeadSessions);
      // expire convotags
      EndpointUtil::ExpireConvoSessions(now, Sessions());
    }

    bool
    Endpoint::Stop()
    {
      // stop remote sessions
      EndpointUtil::StopRemoteSessions(m_state->m_RemoteSessions);
      // stop snode sessions
      EndpointUtil::StopSnodeSessions(m_state->m_SNodeSessions);
      if(m_OnDown)
        m_OnDown->NotifyAsync(NotifyParams());
      return path::Builder::Stop();
    }

    uint64_t
    Endpoint::GenTXID()
    {
      uint64_t txid       = randint();
      const auto& lookups = m_state->m_PendingLookups;
      while(lookups.find(txid) != lookups.end())
        ++txid;
      return txid;
    }

    std::string
    Endpoint::Name() const
    {
      return m_state->m_Name + ":" + m_Identity.pub.Name();
    }

    void
    Endpoint::PutLookup(IServiceLookup* lookup, uint64_t txid)
    {
      m_state->m_PendingLookups.emplace(
          txid, std::unique_ptr< IServiceLookup >(lookup));
    }

    bool
    Endpoint::HandleGotIntroMessage(dht::GotIntroMessage_constptr msg)
    {
      std::set< IntroSet > remote;
      auto currentPub = m_state->m_CurrentPublishTX;
      for(const auto& introset : msg->I)
      {
        if(!introset.Verify(Now()))
        {
          if(m_Identity.pub == introset.A && currentPub == msg->T)
            IntroSetPublishFail();
          return true;
        }
        if(m_Identity.pub == introset.A && currentPub == msg->T)
        {
          LogInfo(
              "got introset publish confirmation for hidden service endpoint ",
              Name());
          IntroSetPublished();
          return true;
        }

        remote.insert(introset);
      }
      auto& lookups = m_state->m_PendingLookups;
      auto itr      = lookups.find(msg->T);
      if(itr == lookups.end())
      {
        LogWarn("invalid lookup response for hidden service endpoint ", Name(),
                " txid=", msg->T);
        return true;
      }
      std::unique_ptr< IServiceLookup > lookup = std::move(itr->second);
      lookups.erase(itr);
      lookup->HandleResponse(remote);
      return true;
    }

    bool
    Endpoint::HasInboundConvo(const Address& addr) const
    {
      for(const auto& item : Sessions())
      {
        if(item.second.remote.Addr() == addr && item.second.inbound)
          return true;
      }
      return false;
    }

    void
    Endpoint::PutSenderFor(const ConvoTag& tag, const ServiceInfo& info,
                           bool inbound)
    {
      auto itr = Sessions().find(tag);
      if(itr == Sessions().end())
      {
        itr                 = Sessions().emplace(tag, Session{}).first;
        itr->second.inbound = inbound;
        itr->second.remote  = info;
      }
      itr->second.lastUsed = Now();
    }

    bool
    Endpoint::GetSenderFor(const ConvoTag& tag, ServiceInfo& si) const
    {
      auto itr = Sessions().find(tag);
      if(itr == Sessions().end())
        return false;
      si = itr->second.remote;
      return true;
    }

    void
    Endpoint::PutIntroFor(const ConvoTag& tag, const Introduction& intro)
    {
      auto itr = Sessions().find(tag);
      if(itr == Sessions().end())
      {
        return;
      }
      itr->second.intro    = intro;
      itr->second.lastUsed = Now();
    }

    bool
    Endpoint::GetIntroFor(const ConvoTag& tag, Introduction& intro) const
    {
      auto itr = Sessions().find(tag);
      if(itr == Sessions().end())
        return false;
      intro = itr->second.intro;
      return true;
    }

    void
    Endpoint::PutReplyIntroFor(const ConvoTag& tag, const Introduction& intro)
    {
      auto itr = Sessions().find(tag);
      if(itr == Sessions().end())
      {
        return;
      }
      itr->second.replyIntro = intro;
      itr->second.lastUsed   = Now();
    }

    bool
    Endpoint::GetReplyIntroFor(const ConvoTag& tag, Introduction& intro) const
    {
      auto itr = Sessions().find(tag);
      if(itr == Sessions().end())
        return false;
      intro = itr->second.replyIntro;
      return true;
    }

    bool
    Endpoint::GetConvoTagsForService(const Address& addr,
                                     std::set< ConvoTag >& tags) const
    {
      return EndpointUtil::GetConvoTagsForService(Sessions(), addr, tags);
    }

    bool
    Endpoint::GetCachedSessionKeyFor(const ConvoTag& tag,
                                     SharedSecret& secret) const
    {
      auto itr = Sessions().find(tag);
      if(itr == Sessions().end())
        return false;
      secret = itr->second.sharedKey;
      return true;
    }

    void
    Endpoint::PutCachedSessionKeyFor(const ConvoTag& tag, const SharedSecret& k)
    {
      auto itr = Sessions().find(tag);
      if(itr == Sessions().end())
      {
        itr = Sessions().emplace(tag, Session{}).first;
      }
      itr->second.sharedKey = k;
      itr->second.lastUsed  = Now();
    }

    void
    Endpoint::MarkConvoTagActive(const ConvoTag& tag)
    {
      auto itr = Sessions().find(tag);
      if(itr != Sessions().end())
      {
        itr->second.lastUsed = Now();
      }
    }

    bool
    Endpoint::LoadKeyFile()
    {
      const auto& keyfile = m_state->m_Keyfile;
      if(!keyfile.empty())
      {
        if(!m_Identity.EnsureKeys(keyfile,
                                  Router()->keyManager()->needBackup()))
        {
          LogError("Can't ensure keyfile [", keyfile, "]");
          return false;
        }
      }
      else
      {
        m_Identity.RegenerateKeys();
      }
      return true;
    }

    bool
    Endpoint::Start()
    {
      // how can I tell if a m_Identity isn't loaded?
      if(!m_DataHandler)
      {
        m_DataHandler = this;
      }
      // this does network isolation
      while(m_state->m_OnInit.size())
      {
        if(m_state->m_OnInit.front()())
          m_state->m_OnInit.pop_front();
        else
        {
          LogWarn("Can't call init of network isolation");
          return false;
        }
      }
      return true;
    }

    Endpoint::~Endpoint()
    {
      if(m_OnUp)
        m_OnUp->Stop();
      if(m_OnDown)
        m_OnDown->Stop();
      if(m_OnReady)
        m_OnReady->Stop();
    }

    bool
    Endpoint::PublishIntroSet(AbstractRouter* r)
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
      PublishIntroSetJob(Endpoint* parent, uint64_t id, IntroSet introset)
          : IServiceLookup(parent, id, "PublishIntroSet")
          , m_IntroSet(std::move(introset))
          , m_Endpoint(parent)
      {
      }

      std::shared_ptr< routing::IMessage >
      BuildRequestMessage() override
      {
        auto msg = std::make_shared< routing::DHTMessage >();
        msg->M.emplace_back(
            std::make_unique< dht::PublishIntroMessage >(m_IntroSet, txid, 5));
        return msg;
      }

      bool
      HandleResponse(const std::set< IntroSet >& response) override
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
      auto now = Now();
      if(ShouldPublishDescriptors(now))
      {
        RegenAndPublishIntroSet();
      }
      else if(NumInStatus(path::ePathEstablished) < 3)
      {
        if(introSet().HasExpiredIntros(now))
          ManualRebuild(1);
      }
    }

    bool
    Endpoint::PublishIntroSetVia(AbstractRouter* r, path::Path_ptr path)
    {
      auto job = new PublishIntroSetJob(this, GenTXID(), introSet());
      if(job->SendRequestViaPath(path, r))
      {
        m_state->m_LastPublishAttempt = Now();
        return true;
      }
      return false;
    }

    void
    Endpoint::ResetInternalState()
    {
      path::Builder::ResetInternalState();
      static auto resetState = [](auto& container, auto getter) {
        std::for_each(container.begin(), container.end(), [getter](auto& item) {
          getter(item)->ResetInternalState();
        });
      };
      resetState(m_state->m_RemoteSessions,
                 [](const auto& item) { return item.second; });
      resetState(m_state->m_SNodeSessions,
                 [](const auto& item) { return item.second.first; });
    }

    bool
    Endpoint::ShouldPublishDescriptors(llarp_time_t now) const
    {
      // make sure we have all paths that are established
      // in our introset
      size_t numNotInIntroset = 0;
      ForEachPath([&](const path::Path_ptr& p) {
        if(!p->IsReady())
          return;
        for(const auto& i : introSet().I)
        {
          if(i == p->intro)
            return;
        }
        ++numNotInIntroset;
      });

      const auto lastpub = m_state->m_LastPublishAttempt;
      if(m_state->m_IntroSet.HasExpiredIntros(now) || numNotInIntroset > 1)
      {
        return now - lastpub >= INTROSET_PUBLISH_RETRY_INTERVAL;
      }
      return now - lastpub >= INTROSET_PUBLISH_INTERVAL;
    }

    void
    Endpoint::IntroSetPublished()
    {
      m_state->m_LastPublish = Now();
      LogInfo(Name(), " IntroSet publish confirmed");
      if(m_OnReady)
        m_OnReady->NotifyAsync(NotifyParams());
      m_OnReady = nullptr;
    }

    void
    Endpoint::IsolatedNetworkMainLoop()
    {
      m_state->m_IsolatedNetLoop = llarp_make_ev_loop();
      m_state->m_IsolatedLogic   = std::make_shared< llarp::Logic >();
      if(SetupNetworking())
        llarp_ev_loop_run_single_process(m_state->m_IsolatedNetLoop,
                                         m_state->m_IsolatedLogic);
      else
      {
        m_state->m_IsolatedNetLoop.reset();
        m_state->m_IsolatedLogic.reset();
      }
    }

    bool
    Endpoint::SelectHop(llarp_nodedb* db, const std::set< RouterID >& prev,
                        RouterContact& cur, size_t hop, path::PathRole roles)

    {
      std::set< RouterID > exclude = prev;
      for(const auto& snode : SnodeBlacklist())
        exclude.insert(snode);
      if(hop == 0)
      {
        const auto exits = GetExitRouters();
        // exclude exit node as first hop in any paths
        exclude.insert(exits.begin(), exits.end());
      }
      return path::Builder::SelectHop(db, exclude, cur, hop, roles);
    }

    std::set< RouterID >
    Endpoint::GetExitRouters() const
    {
      return m_ExitMap.TransformValues< RouterID >(
          [](const exit::BaseSession_ptr& ptr) -> RouterID {
            return ptr->Endpoint();
          });
    }

    bool
    Endpoint::ShouldBundleRC() const
    {
      return m_state->m_BundleRC;
    }

    void
    Endpoint::PutNewOutboundContext(const service::IntroSet& introset)
    {
      Address addr;
      introset.A.CalculateAddress(addr.as_array());

      auto& remoteSessions = m_state->m_RemoteSessions;
      auto& serviceLookups = m_state->m_PendingServiceLookups;

      if(remoteSessions.count(addr) >= MAX_OUTBOUND_CONTEXT_COUNT)
      {
        auto itr = remoteSessions.find(addr);

        auto range = serviceLookups.equal_range(addr);
        auto i     = range.first;
        if(i != range.second)
        {
          i->second(addr, itr->second.get());
          ++i;
        }
        serviceLookups.erase(addr);
        return;
      }

      auto it = remoteSessions.emplace(
          addr, std::make_shared< OutboundContext >(introset, this));
      LogInfo("Created New outbound context for ", addr.ToString());

      // inform pending
      auto range = serviceLookups.equal_range(addr);
      auto itr   = range.first;
      if(itr != range.second)
      {
        itr->second(addr, it->second.get());
        ++itr;
      }
      serviceLookups.erase(addr);
    }

    void
    Endpoint::HandleVerifyGotRouter(dht::GotRouterMessage_constptr msg,
                                    llarp_async_verify_rc* j)
    {
      auto& pendingRouters = m_state->m_PendingRouters;
      auto itr             = pendingRouters.find(msg->R[0].pubkey);
      if(itr != pendingRouters.end())
      {
        if(j->valid)
          itr->second.InformResult(msg->R);
        else
          itr->second.InformResult({});
        pendingRouters.erase(itr);
      }
      delete j;
    }

    bool
    Endpoint::HandleGotRouterMessage(dht::GotRouterMessage_constptr msg)
    {
      if(msg->R.size())
      {
        auto* job         = new llarp_async_verify_rc;
        job->nodedb       = Router()->nodedb();
        job->cryptoworker = Router()->threadpool();
        job->diskworker   = Router()->diskworker();
        job->logic        = Router()->logic();
        job->hook = std::bind(&Endpoint::HandleVerifyGotRouter, this, msg,
                              std::placeholders::_1);
        job->rc   = msg->R[0];
        llarp_nodedb_async_verify(job);
      }
      else
      {
        auto& routers = m_state->m_PendingRouters;
        auto itr      = routers.begin();
        while(itr != routers.end())
        {
          if(itr->second.txid == msg->txid)
          {
            itr->second.InformResult({});
            itr = routers.erase(itr);
          }
          else
            ++itr;
        }
      }
      return true;
    }

    void
    Endpoint::EnsureRouterIsKnown(const RouterID& router)
    {
      if(router.IsZero())
        return;
      if(!Router()->nodedb()->Has(router))
      {
        LookupRouterAnon(router, nullptr);
      }
    }

    bool
    Endpoint::LookupRouterAnon(RouterID router, RouterLookupHandler handler)
    {
      auto& routers = m_state->m_PendingRouters;
      if(routers.find(router) == routers.end())
      {
        auto path = GetEstablishedPathClosestTo(router);
        routing::DHTMessage msg;
        auto txid = GenTXID();
        msg.M.emplace_back(
            std::make_unique< dht::FindRouterMessage >(txid, router));

        if(path && path->SendRoutingMessage(msg, Router()))
        {
          routers.emplace(router, RouterLookupJob(this, handler));
          return true;
        }
      }
      return false;
    }

    void
    Endpoint::HandlePathBuilt(path::Path_ptr p)
    {
      p->SetDataHandler(util::memFn(&Endpoint::HandleHiddenServiceFrame, this));
      p->SetDropHandler(util::memFn(&Endpoint::HandleDataDrop, this));
      p->SetDeadChecker(util::memFn(&Endpoint::CheckPathIsDead, this));
      path::Builder::HandlePathBuilt(p);
    }

    bool
    Endpoint::HandleDataDrop(path::Path_ptr p, const PathID_t& dst,
                             uint64_t seq)
    {
      LogWarn(Name(), " message ", seq, " dropped by endpoint ", p->Endpoint(),
              " via ", dst);
      return true;
    }

    std::unordered_map< std::string, std::string >
    Endpoint::NotifyParams() const
    {
      return {{"LOKINET_ADDR", m_Identity.pub.Addr().ToString()}};
    }

    void
    Endpoint::FlushRecvData()
    {
      do
      {
        auto maybe = m_RecvQueue.tryPopFront();
        if(not maybe.has_value())
          return;
        auto ev = std::move(maybe.value());
        ProtocolMessage::ProcessAsync(ev.fromPath, ev.pathid, ev.msg);
      } while(true);
    }

    void
    Endpoint::QueueRecvData(RecvDataEvent ev)
    {
      if(m_RecvQueue.full() || m_RecvQueue.empty())
      {
        auto self = this;
        LogicCall(m_router->logic(), [self]() { self->FlushRecvData(); });
      }
      m_RecvQueue.pushBack(std::move(ev));
    }

    bool
    Endpoint::HandleDataMessage(path::Path_ptr path, const PathID_t from,
                                std::shared_ptr< ProtocolMessage > msg)
    {
      msg->sender.UpdateAddr();
      PutSenderFor(msg->tag, msg->sender, true);
      PutReplyIntroFor(msg->tag, path->intro);
      Introduction intro;
      intro.pathID    = from;
      intro.router    = PubKey(path->Endpoint());
      intro.expiresAt = std::min(path->ExpireTime(), msg->introReply.expiresAt);
      PutIntroFor(msg->tag, intro);
      return ProcessDataMessage(msg);
    }

    bool
    Endpoint::HasPathToSNode(const RouterID ident) const
    {
      auto range = m_state->m_SNodeSessions.equal_range(ident);
      auto itr   = range.first;
      while(itr != range.second)
      {
        if(itr->second.first->IsReady())
        {
          return true;
        }
        ++itr;
      }
      return false;
    }

    bool
    Endpoint::ProcessDataMessage(std::shared_ptr< ProtocolMessage > msg)
    {
      if(msg->proto == eProtocolTrafficV4 || msg->proto == eProtocolTrafficV6)
      {
        util::Lock l(&m_state->m_InboundTrafficQueueMutex);
        m_state->m_InboundTrafficQueue.emplace(msg);
        return true;
      }
      if(msg->proto == eProtocolControl)
      {
        // TODO: implement me (?)
        // right now it's just random noise
        return true;
      }
      return false;
    }

    void
    Endpoint::RemoveConvoTag(const ConvoTag& t)
    {
      Sessions().erase(t);
    }

    bool
    Endpoint::HandleHiddenServiceFrame(path::Path_ptr p,
                                       const ProtocolFrame& frame)
    {
      if(frame.R)
      {
        // handle discard
        ServiceInfo si;
        if(!GetSenderFor(frame.T, si))
          return false;
        // verify source
        if(!frame.Verify(si))
          return false;
        // remove convotag it doesn't exist
        LogWarn("remove convotag T=", frame.T);
        RemoveConvoTag(frame.T);
        return true;
      }
      if(!frame.AsyncDecryptAndVerify(EndpointLogic(), p, CryptoWorker(),
                                      m_Identity, m_DataHandler))
      {
        // send discard
        ProtocolFrame f;
        f.R = 1;
        f.T = frame.T;
        f.F = p->intro.pathID;

        if(!f.Sign(m_Identity))
          return false;
        {
          util::Lock lock(&m_state->m_SendQueueMutex);
          m_state->m_SendQueue.emplace_back(
              std::make_shared< const routing::PathTransferMessage >(f,
                                                                     frame.F),
              p);
        }
        return true;
      }
      return true;
    }

    void Endpoint::HandlePathDied(path::Path_ptr)
    {
      RegenAndPublishIntroSet(true);
    }

    bool
    Endpoint::CheckPathIsDead(path::Path_ptr, llarp_time_t dlt)
    {
      return dlt > path::alive_timeout;
    }

    bool
    Endpoint::OnLookup(const Address& addr, const IntroSet* introset,
                       const RouterID& endpoint)
    {
      const auto now = Router()->Now();
      auto& fails    = m_state->m_ServiceLookupFails;
      auto& lookups  = m_state->m_PendingServiceLookups;
      if(introset == nullptr || introset->IsExpired(now))
      {
        LogError(Name(), " failed to lookup ", addr.ToString(), " from ",
                 endpoint);
        fails[endpoint] = fails[endpoint] + 1;
        // inform all
        auto range = lookups.equal_range(addr);
        auto itr   = range.first;
        if(itr != range.second)
        {
          itr->second(addr, nullptr);
          itr = lookups.erase(itr);
        }
        return false;
      }

      PutNewOutboundContext(*introset);
      return true;
    }

    bool
    Endpoint::EnsurePathToService(const Address remote, PathEnsureHook hook,
                                  ABSL_ATTRIBUTE_UNUSED llarp_time_t timeoutMS,
                                  bool randomPath)
    {
      path::Path_ptr path = nullptr;
      if(randomPath)
        path = PickRandomEstablishedPath();
      else
        path = GetEstablishedPathClosestTo(remote.ToRouter());
      if(!path)
      {
        LogWarn("No outbound path for lookup yet");
        BuildOne();
        return false;
      }

      LogInfo(Name(), " Ensure Path to ", remote.ToString());

      auto& sessions = m_state->m_RemoteSessions;

      {
        auto itr = sessions.find(remote);
        if(itr != sessions.end())
        {
          hook(itr->first, itr->second.get());
          return true;
        }
      }

      auto& lookups = m_state->m_PendingServiceLookups;

      if(lookups.count(remote) >= MaxConcurrentLookups)
      {
        LogWarn(Name(), " has too many pending service lookups for ",
                remote.ToString());
        return false;
      }

      using namespace std::placeholders;
      HiddenServiceAddressLookup* job = new HiddenServiceAddressLookup(
          this, util::memFn(&Endpoint::OnLookup, this), remote, GenTXID());
      LogInfo("doing lookup for ", remote, " via ", path->Endpoint());
      if(job->SendRequestViaPath(path, Router()))
      {
        lookups.emplace(remote, hook);
        return true;
      }
      LogError("send via path failed");
      return false;
    }

    void
    Endpoint::EnsurePathToSNode(const RouterID snode, SNodeEnsureHook h)
    {
      auto& nodeSessions = m_state->m_SNodeSessions;
      using namespace std::placeholders;
      if(nodeSessions.count(snode) == 0)
      {
        ConvoTag tag;
        // TODO: check for collision lol no we don't but maybe we will...
        // some day :DDDDD
        tag.Randomize();
        auto session = std::make_shared< exit::SNodeSession >(
            snode,
            [=](const llarp_buffer_t& pkt) -> bool {
              /// TODO: V6
              return HandleInboundPacket(tag, pkt, eProtocolTrafficV4);
            },
            Router(), numPaths, numHops, false, ShouldBundleRC());

        m_state->m_SNodeSessions.emplace(snode, std::make_pair(session, tag));
      }
      EnsureRouterIsKnown(snode);
      auto range = nodeSessions.equal_range(snode);
      auto itr   = range.first;
      while(itr != range.second)
      {
        if(itr->second.first->IsReady())
          h(snode, itr->second.first);
        else
        {
          itr->second.first->AddReadyHook(std::bind(h, snode, _1));
          itr->second.first->BuildOne();
        }
        ++itr;
      }
    }

    bool
    Endpoint::SendToSNodeOrQueue(const RouterID& addr,
                                 const llarp_buffer_t& buf)
    {
      auto pkt = std::make_shared< net::IPPacket >();
      if(!pkt->Load(buf))
        return false;
      EnsurePathToSNode(addr, [pkt](RouterID, exit::BaseSession_ptr s) {
        if(s)
          s->QueueUpstreamTraffic(*pkt, routing::ExitPadSize);
      });
      return true;
    }

    void Endpoint::Pump(llarp_time_t)
    {
      const auto& sessions = m_state->m_SNodeSessions;
      auto& queue          = m_state->m_InboundTrafficQueue;

      auto epPump = [&]() {
        FlushRecvData();
        // send downstream packets to user for snode
        for(const auto& item : sessions)
          item.second.first->FlushDownstream();
        // send downstream traffic to user for hidden service
        util::Lock lock(&m_state->m_InboundTrafficQueueMutex);
        while(not queue.empty())
        {
          const auto& msg = queue.top();
          const llarp_buffer_t buf(msg->payload);
          HandleInboundPacket(msg->tag, buf, msg->proto);
          queue.pop();
        }
      };

      if(NetworkIsIsolated())
      {
        LogicCall(EndpointLogic(), epPump);
      }
      else
      {
        epPump();
      }

      auto router = Router();
      // TODO: locking on this container
      for(const auto& item : m_state->m_RemoteSessions)
        item.second->FlushUpstream();
      // TODO: locking on this container
      for(const auto& item : sessions)
        item.second.first->FlushUpstream();
      {
        util::Lock lock(&m_state->m_SendQueueMutex);
        // send outbound traffic
        for(const auto& item : m_state->m_SendQueue)
        {
          item.second->SendRoutingMessage(*item.first, router);
          MarkConvoTagActive(item.first->T.T);
        }
        m_state->m_SendQueue.clear();
      }
      router->PumpLL();
    }

    bool
    Endpoint::EnsureConvo(ABSL_ATTRIBUTE_UNUSED const AlignedBuffer< 32 > addr,
                          bool snode,
                          ABSL_ATTRIBUTE_UNUSED ConvoEventListener_ptr ev)
    {
      if(snode)
      {
      }

      // TODO: something meaningful
      return false;
    }

    bool
    Endpoint::SendToServiceOrQueue(const service::Address& remote,
                                   const llarp_buffer_t& data, ProtocolType t)
    {
      if(data.sz == 0)
        return false;
      // inbound converstation
      const auto now = Now();

      if(HasInboundConvo(remote))
      {
        auto transfer    = std::make_shared< routing::PathTransferMessage >();
        ProtocolFrame& f = transfer->T;
        std::shared_ptr< path::Path > p;
        std::set< ConvoTag > tags;
        if(GetConvoTagsForService(remote, tags))
        {
          // the remote guy's intro
          Introduction remoteIntro;
          Introduction replyPath;
          SharedSecret K;
          // pick tag
          for(const auto& tag : tags)
          {
            if(tag.IsZero())
              continue;
            if(!GetCachedSessionKeyFor(tag, K))
              continue;
            if(!GetReplyIntroFor(tag, replyPath))
              continue;
            if(!GetIntroFor(tag, remoteIntro))
              continue;
            // get path for intro
            ForEachPath([&](path::Path_ptr path) {
              if(path->intro == replyPath)
              {
                p = path;
                return;
              }
              if(p && p->ExpiresSoon(now) && path->IsReady()
                 && path->intro.router == replyPath.router)
              {
                p = path;
              }
            });
            if(p)
            {
              f.T = tag;
            }
          }
          if(p)
          {
            // TODO: check expiration of our end
            auto m = std::make_shared< ProtocolMessage >(f.T);
            m->PutBuffer(data);
            f.N.Randomize();
            f.C.Zero();
            transfer->Y.Randomize();
            m->proto      = t;
            m->introReply = p->intro;
            PutReplyIntroFor(f.T, m->introReply);
            m->sender   = m_Identity.pub;
            m->seqno    = GetSeqNoForConvo(f.T);
            f.S         = 1;
            f.F         = m->introReply.pathID;
            transfer->P = remoteIntro.pathID;
            auto self   = this;
            return CryptoWorker()->addJob([transfer, p, m, K, self]() {
              if(not transfer->T.EncryptAndSign(*m, K, self->m_Identity))
              {
                LogError("failed to encrypt and sign");
                return;
              }

              util::Lock lock(&self->m_state->m_SendQueueMutex);
              self->m_state->m_SendQueue.emplace_back(transfer, p);
            });
          }
        }
      }

      // outbound converstation
      auto& sessions = m_state->m_RemoteSessions;
      if(EndpointUtil::HasPathToService(remote, sessions))
      {
        auto range = sessions.equal_range(remote);
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

      auto& traffic = m_state->m_PendingTraffic;
      traffic[remote].emplace_back(data, t);
      // no converstation
      return EnsurePathToService(
          remote,
          [&](Address r, OutboundContext* c) {
            if(c)
            {
              c->UpdateIntroSet(true);
              for(auto& pending : m_state->m_PendingTraffic[r])
              {
                c->AsyncEncryptAndSendTo(pending.Buffer(), pending.protocol);
              }
            }
            m_state->m_PendingTraffic.erase(r);
          },
          5000);
    }

    bool
    Endpoint::HasConvoTag(const ConvoTag& t) const
    {
      return Sessions().find(t) != Sessions().end();
    }

    uint64_t
    Endpoint::GetSeqNoForConvo(const ConvoTag& tag)
    {
      auto itr = Sessions().find(tag);
      if(itr == Sessions().end())
        return 0;
      return ++(itr->second.seqno);
    }

    bool
    Endpoint::ShouldBuildMore(llarp_time_t now) const
    {
      if(path::Builder::BuildCooldownHit(now))
        return false;
      const bool should = path::Builder::ShouldBuildMore(now);
      // determine newest intro
      Introduction intro;
      if(!GetNewestIntro(intro))
        return should;
      // time from now that the newest intro expires at
      if(intro.ExpiresSoon(now))
        return should;

      const auto dlt = now - (intro.expiresAt - path::default_lifetime);

      return should
          || (  // try spacing tunnel builds out evenly in time
                 (dlt >= (path::default_lifetime / 4))
                 && (NumInStatus(path::ePathBuilding) < numPaths));
    }

    std::shared_ptr< Logic >
    Endpoint::RouterLogic()
    {
      return Router()->logic();
    }

    std::shared_ptr< Logic >
    Endpoint::EndpointLogic()
    {
      return m_state->m_IsolatedLogic ? m_state->m_IsolatedLogic
                                      : Router()->logic();
    }

    std::shared_ptr< llarp::thread::ThreadPool >
    Endpoint::CryptoWorker()
    {
      return Router()->threadpool();
    }

    AbstractRouter*
    Endpoint::Router()
    {
      return m_state->m_Router;
    }

    const std::set< RouterID >&
    Endpoint::SnodeBlacklist() const
    {
      return m_state->m_SnodeBlacklist;
    }

    const IntroSet&
    Endpoint::introSet() const
    {
      return m_state->m_IntroSet;
    }

    IntroSet&
    Endpoint::introSet()
    {
      return m_state->m_IntroSet;
    }

    const ConvoMap&
    Endpoint::Sessions() const
    {
      return m_state->m_Sessions;
    }

    ConvoMap&
    Endpoint::Sessions()
    {
      return m_state->m_Sessions;
    }
  }  // namespace service
}  // namespace llarp
