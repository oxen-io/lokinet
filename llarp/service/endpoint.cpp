#include <service/endpoint.hpp>

#include <dht/messages/findintro.hpp>
#include <dht/messages/findrouter.hpp>
#include <dht/messages/gotintro.hpp>
#include <dht/messages/gotrouter.hpp>
#include <dht/messages/pubintro.hpp>
#include <messages/dht.hpp>
#include <messages/path_transfer.hpp>
#include <nodedb.hpp>
#include <profiling.hpp>
#include <router/abstractrouter.hpp>
#include <service/endpoint_util.hpp>
#include <service/hidden_service_address_lookup.hpp>
#include <service/outbound_context.hpp>
#include <service/protocol.hpp>
#include <util/logic.hpp>
#include <util/str.hpp>
#include <util/buffer.hpp>
#include <util/memfn.hpp>
#include <hook/shell.hpp>

namespace llarp
{
  namespace service
  {
    Endpoint::Endpoint(const std::string& name, AbstractRouter* r,
                       Context* parent)
        : path::Builder(r, r->dht(), 3, path::default_len)
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
        LogInfo("Setting tag to ", v);
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
      if(k == "bundle-rc")
      {
        m_BundleRC = IsTrueValue(v.c_str());
      }
      if(k == "blacklist-snode")
      {
        RouterID snode;
        if(!snode.FromString(v))
        {
          LogError(Name(), " invalid snode value: ", v);
          return false;
        }
        const auto result = m_SnodeBlacklist.insert(snode);
        if(!result.second)
        {
          LogError(Name(), " duplicate blacklist-snode: ", snode.ToString());
          return false;
        }
        LogInfo(Name(), " adding ", snode.ToString(), " to blacklist");
      }
      if(k == "on-up")
      {
        m_OnUp = hooks::ExecShellBackend(v);
        if(m_OnUp)
          LogInfo(Name(), " added on up script: ", v);
        else
          LogError(Name(), " failed to add on up script");
      }
      if(k == "on-down")
      {
        m_OnDown = hooks::ExecShellBackend(v);
        if(m_OnDown)
          LogInfo(Name(), " added on down script: ", v);
        else
          LogError(Name(), " failed to add on down script");
      }
      if(k == "on-ready")
      {
        m_OnReady = hooks::ExecShellBackend(v);
        if(m_OnReady)
          LogInfo(Name(), " added on ready script: ", v);
        else
          LogError(Name(), " failed to add on ready script");
      }
      return true;
    }

    llarp_ev_loop_ptr
    Endpoint::EndpointNetLoop()
    {
      if(m_IsolatedNetLoop)
        return m_IsolatedNetLoop;
      else
        return m_Router->netloop();
    }

    bool
    Endpoint::NetworkIsIsolated() const
    {
      return m_IsolatedLogic.get() != nullptr && m_IsolatedNetLoop != nullptr;
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
        LogWarn("could not publish descriptors for endpoint ", Name(),
                " because we couldn't get enough valid introductions");
        if(ShouldBuildMore(now) || forceRebuild)
          ManualRebuild(1);
        return;
      }
      m_IntroSet.I.clear();
      for(auto& intro : I)
      {
        m_IntroSet.I.emplace_back(std::move(intro));
      }
      if(m_IntroSet.I.size() == 0)
      {
        LogWarn("not enough intros to publish introset for ", Name());
        if(ShouldBuildMore(now) || forceRebuild)
          ManualRebuild(1);
        return;
      }
      m_IntroSet.topic = m_Tag;
      if(!m_Identity.SignIntroSet(m_IntroSet, now))
      {
        LogWarn("failed to sign introset for endpoint ", Name());
        return;
      }
      if(PublishIntroSet(m_Router))
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
      if(m_IntroSet.I.size() == 0)
        return false;
      if(m_IntroSet.IsExpired(now))
        return false;
      return true;
    }

    bool
    Endpoint::HasPendingRouterLookup(const RouterID remote) const
    {
      return m_PendingRouters.find(remote) != m_PendingRouters.end();
    }

    bool
    Endpoint::IntrosetIsStale() const
    {
      return m_IntroSet.HasExpiredIntros(Now());
    }

    util::StatusObject
    Endpoint::ExtractStatus() const
    {
      auto obj = path::Builder::ExtractStatus();
      obj.Put("identity", m_Identity.pub.Addr().ToString());

      obj.Put("lastPublished", m_LastPublish);
      obj.Put("lastPublishAttempt", m_LastPublishAttempt);
      obj.Put("introset", m_IntroSet.ExtractStatus());

      if(!m_Tag.IsZero())
        obj.Put("tag", m_Tag.ToString());

      obj.PutContainer("deadSessions", m_DeadSessions);
      obj.PutContainer("remoteSessions", m_RemoteSessions);
      obj.PutContainer("snodeSessions", m_SNodeSessions);
      obj.PutContainer("lookups", m_PendingLookups);

      util::StatusObject sessionObj{};

      for(const auto& item : m_Sessions)
      {
        std::string k = item.first.ToHex();
        sessionObj.Put(k, item.second.ExtractStatus());
      }

      obj.Put("converstations", sessionObj);
      return obj;
    }

    void
    Endpoint::Tick(llarp_time_t now)
    {
      path::Builder::Tick(now);
      // publish descriptors
      if(ShouldPublishDescriptors(now))
      {
        RegenAndPublishIntroSet(now);
      }
      else if(NumInStatus(path::ePathEstablished) < 3)
      {
        if(m_IntroSet.HasExpiredIntros(now))
          ManualRebuild(1);
      }

      // expire snode sessions
      EndpointUtil::ExpireSNodeSessions(now, m_SNodeSessions);
      // expire pending tx
      EndpointUtil::ExpirePendingTx(now, m_PendingLookups);
      // expire pending router lookups
      EndpointUtil::ExpirePendingRouterLookups(now, m_PendingRouters);

      // prefetch addrs
      for(const auto& addr : m_PrefetchAddrs)
      {
        if(!EndpointUtil::HasPathToService(addr, m_RemoteSessions))
        {
          if(!EnsurePathToService(
                 addr,
                 [](ABSL_ATTRIBUTE_UNUSED Address addr,
                    ABSL_ATTRIBUTE_UNUSED OutboundContext* ctx) {},
                 10000))
          {
            LogWarn("failed to ensure path to ", addr);
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
          itr = m_PrefetchedTags.emplace(tag, CachedTagResult(tag, this)).first;
        }
        for(const auto& introset : itr->second.result)
        {
          if(HasPendingPathToService(introset.A.Addr()))
            continue;
          std::array< byte_t, 128 > tmp = {0};
          llarp_buffer_t buf(tmp);
          if(SendToServiceOrQueue(introset.A.Addr().data(), buf,
                                  eProtocolControl))
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
      EndpointUtil::DeregisterDeadSessions(now, m_DeadSessions);
      // tick remote sessions
      EndpointUtil::TickRemoteSessions(now, m_RemoteSessions, m_DeadSessions);
      // expire convotags
      EndpointUtil::ExpireConvoSessions(now, m_Sessions);
    }

    bool
    Endpoint::Stop()
    {
      // stop remote sessions
      EndpointUtil::StopRemoteSessions(m_RemoteSessions);
      // stop snode sessions
      EndpointUtil::StopSnodeSessions(m_SNodeSessions);
      if(m_OnDown)
        m_OnDown->NotifyAsync(NotifyParams());
      return path::Builder::Stop();
    }

    uint64_t
    Endpoint::GenTXID()
    {
      uint64_t txid = randint();
      while(m_PendingLookups.find(txid) != m_PendingLookups.end())
        ++txid;
      return txid;
    }

    std::string
    Endpoint::Name() const
    {
      return m_Name + ":" + m_Identity.pub.Name();
    }

    void
    Endpoint::PutLookup(IServiceLookup* lookup, uint64_t txid)
    {
      // std::unique_ptr< service::IServiceLookup > ptr(lookup);
      // m_PendingLookups.emplace(txid, ptr);
      // m_PendingLookups[txid] = std::move(ptr);
      m_PendingLookups.emplace(txid, std::unique_ptr< IServiceLookup >(lookup));
    }

    bool
    Endpoint::HandleGotIntroMessage(dht::GotIntroMessage_constptr msg)
    {
      std::set< IntroSet > remote;
      for(const auto& introset : msg->I)
      {
        if(!introset.Verify(Now()))
        {
          if(m_Identity.pub == introset.A && m_CurrentPublishTX == msg->T)
            IntroSetPublishFail();
          return true;
        }
        if(m_Identity.pub == introset.A && m_CurrentPublishTX == msg->T)
        {
          LogInfo(
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
        LogWarn("invalid lookup response for hidden service endpoint ", Name(),
                " txid=", msg->T);
        return true;
      }
      std::unique_ptr< IServiceLookup > lookup = std::move(itr->second);
      m_PendingLookups.erase(itr);
      lookup->HandleResponse(remote);
      return true;
    }

    bool
    Endpoint::HasInboundConvo(const Address& addr) const
    {
      for(const auto& item : m_Sessions)
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
      auto itr = m_Sessions.find(tag);
      if(itr == m_Sessions.end())
      {
        itr = m_Sessions.emplace(tag, Session{}).first;
      }
      itr->second.remote   = info;
      itr->second.inbound  = inbound;
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
        return;
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

    void
    Endpoint::PutReplyIntroFor(const ConvoTag& tag, const Introduction& intro)
    {
      auto itr = m_Sessions.find(tag);
      if(itr == m_Sessions.end())
      {
        return;
      }
      itr->second.replyIntro = intro;
      itr->second.lastUsed   = Now();
    }

    bool
    Endpoint::GetReplyIntroFor(const ConvoTag& tag, Introduction& intro) const
    {
      auto itr = m_Sessions.find(tag);
      if(itr == m_Sessions.end())
        return false;
      intro = itr->second.replyIntro;
      return true;
    }

    bool
    Endpoint::GetConvoTagsForService(const Address& addr,
                                     std::set< ConvoTag >& tags) const
    {
      return EndpointUtil::GetConvoTagsForService(m_Sessions, addr, tags);
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
        itr = m_Sessions.emplace(tag, Session{}).first;
      }
      itr->second.sharedKey = k;
      itr->second.lastUsed  = Now();
    }

    bool
    Endpoint::LoadKeyFile()
    {
      if(!m_Keyfile.empty())
      {
        if(!m_Identity.EnsureKeys(m_Keyfile))
        {
          LogError("Can't ensure keyfile [", m_Keyfile, "]");
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
      while(m_OnInit.size())
      {
        if(m_OnInit.front()())
          m_OnInit.pop_front();
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
      PublishIntroSetJob(Endpoint* parent, uint64_t id,
                         const IntroSet& introset)
          : IServiceLookup(parent, id, "PublishIntroSet")
          , m_IntroSet(introset)
          , m_Endpoint(parent)
      {
      }

      std::shared_ptr< routing::IMessage >
      BuildRequestMessage()
      {
        auto msg = std::make_shared< routing::DHTMessage >();
        msg->M.emplace_back(
            std::make_unique< dht::PublishIntroMessage >(m_IntroSet, txid, 1));
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
      auto now = Now();
      if(ShouldPublishDescriptors(now))
      {
        RegenAndPublishIntroSet(now);
      }
      else if(NumInStatus(path::ePathEstablished) < 3)
      {
        if(m_IntroSet.HasExpiredIntros(now))
          ManualRebuild(1);
      }
    }

    bool
    Endpoint::PublishIntroSetVia(AbstractRouter* r, path::Path_ptr path)
    {
      auto job = new PublishIntroSetJob(this, GenTXID(), m_IntroSet);
      if(job->SendRequestViaPath(path, r))
      {
        m_LastPublishAttempt = Now();
        return true;
      }
      return false;
    }

    void
    Endpoint::ResetInternalState()
    {
      path::Builder::ResetInternalState();
      static auto resetState = [](auto& container) {
        std::for_each(container.begin(), container.end(),
                      [](auto& item) { item.second->ResetInternalState(); });
      };
      resetState(m_RemoteSessions);
      resetState(m_SNodeSessions);
    }

    bool
    Endpoint::ShouldPublishDescriptors(llarp_time_t now) const
    {
      if(NumInStatus(path::ePathEstablished) < 3)
        return false;
      // make sure we have all paths that are established
      // in our introset
      bool should = false;
      ForEachPath([&](const path::Path_ptr& p) {
        if(!p->IsReady())
          return;
        for(const auto& i : m_IntroSet.I)
        {
          if(i == p->intro)
            return;
        }
        should = true;
      });
      if(m_IntroSet.HasExpiredIntros(now) || should)
        return now - m_LastPublishAttempt >= INTROSET_PUBLISH_RETRY_INTERVAL;
      return now - m_LastPublishAttempt >= INTROSET_PUBLISH_INTERVAL;
    }

    void
    Endpoint::IntroSetPublished()
    {
      m_LastPublish = Now();
      LogInfo(Name(), " IntroSet publish confirmed");
      if(m_OnReady)
        m_OnReady->NotifyAsync(NotifyParams());
      m_OnReady = nullptr;
    }

    void
    Endpoint::IsolatedNetworkMainLoop()
    {
      m_IsolatedNetLoop = llarp_make_ev_loop();
      m_IsolatedLogic   = std::make_shared< llarp::Logic >();
      if(SetupNetworking())
        llarp_ev_loop_run_single_process(
            m_IsolatedNetLoop, m_IsolatedLogic->thread, m_IsolatedLogic);
      else
      {
        m_IsolatedNetLoop.reset();
        m_IsolatedLogic.reset();
      }
    }

    bool
    Endpoint::SelectHop(llarp_nodedb* db, const std::set< RouterID >& prev,
                        RouterContact& cur, size_t hop, path::PathRole roles)

    {
      std::set< RouterID > exclude = prev;
      for(const auto& snode : m_SnodeBlacklist)
        exclude.insert(snode);
      return path::Builder::SelectHop(db, exclude, cur, hop, roles);
    }

    bool
    Endpoint::ShouldBundleRC() const
    {
      return m_BundleRC;
    }

    void
    Endpoint::PutNewOutboundContext(const service::IntroSet& introset)
    {
      Address addr;
      introset.A.CalculateAddress(addr.as_array());

      if(m_RemoteSessions.count(addr) >= MAX_OUTBOUND_CONTEXT_COUNT)
      {
        auto itr = m_RemoteSessions.find(addr);

        auto range = m_PendingServiceLookups.equal_range(addr);
        auto i     = range.first;
        if(i != range.second)
        {
          i->second(addr, itr->second.get());
          ++i;
        }
        m_PendingServiceLookups.erase(addr);
        return;
      }

      auto it = m_RemoteSessions.emplace(
          addr, std::make_shared< OutboundContext >(introset, this));
      LogInfo("Created New outbound context for ", addr.ToString());

      // inform pending
      auto range = m_PendingServiceLookups.equal_range(addr);
      auto itr   = range.first;
      if(itr != range.second)
      {
        itr->second(addr, it->second.get());
        ++itr;
      }
      m_PendingServiceLookups.erase(addr);
    }

    void
    Endpoint::HandleVerifyGotRouter(dht::GotRouterMessage_constptr msg,
                                    llarp_async_verify_rc* j)
    {
      auto itr = m_PendingRouters.find(msg->R[0].pubkey);
      if(itr != m_PendingRouters.end())
      {
        if(j->valid)
          itr->second.InformResult(msg->R);
        else
          itr->second.InformResult({});
        m_PendingRouters.erase(itr);
      }
      delete j;
    }

    bool
    Endpoint::HandleGotRouterMessage(dht::GotRouterMessage_constptr msg)
    {
      if(msg->R.size())
      {
        llarp_async_verify_rc* job = new llarp_async_verify_rc;
        job->nodedb                = m_Router->nodedb();
        job->cryptoworker          = m_Router->threadpool();
        job->diskworker            = m_Router->diskworker();
        job->logic                 = m_Router->logic();
        job->hook = std::bind(&Endpoint::HandleVerifyGotRouter, this, msg,
                              std::placeholders::_1);
        job->rc   = msg->R[0];
        llarp_nodedb_async_verify(job);
      }
      else
      {
        auto itr = m_PendingRouters.begin();
        while(itr != m_PendingRouters.end())
        {
          if(itr->second.txid == msg->txid)
          {
            itr->second.InformResult({});
            itr = m_PendingRouters.erase(itr);
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
      if(!m_Router->nodedb()->Has(router))
      {
        LookupRouterAnon(router, nullptr);
      }
    }

    bool
    Endpoint::LookupRouterAnon(RouterID router, RouterLookupHandler handler)
    {
      if(m_PendingRouters.find(router) == m_PendingRouters.end())
      {
        auto path = GetEstablishedPathClosestTo(router);
        routing::DHTMessage msg;
        auto txid = GenTXID();
        msg.M.emplace_back(
            std::make_unique< dht::FindRouterMessage >(txid, router));

        if(path && path->SendRoutingMessage(msg, m_Router))
        {
          LogInfo(Name(), " looking up ", router);
          m_PendingRouters.emplace(router, RouterLookupJob(this, handler));
          return true;
        }
        else
          LogError("failed to send request for router lookup");
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

    bool
    Endpoint::HandleDataMessage(const PathID_t& src,
                                std::shared_ptr< ProtocolMessage > msg)
    {
      msg->sender.UpdateAddr();
      auto path = GetPathByID(src);
      if(path == nullptr)
        return false;
      PutReplyIntroFor(msg->tag, path->intro);
      PutSenderFor(msg->tag, msg->sender, true);
      PutIntroFor(msg->tag, msg->introReply);
      return ProcessDataMessage(msg);
    }

    bool
    Endpoint::HasPathToSNode(const RouterID& ident) const
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
    Endpoint::ProcessDataMessage(std::shared_ptr< ProtocolMessage > msg)
    {
      if(msg->proto == eProtocolTrafficV4 || msg->proto == eProtocolTrafficV6)
      {
        util::Lock l(&m_InboundTrafficQueueMutex);
        m_InboundTrafficQueue.emplace(msg);
        return true;
      }
      else if(msg->proto == eProtocolControl)
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
      m_Sessions.erase(t);
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
          util::Lock lock(&m_SendQueueMutex);
          m_SendQueue.emplace_back(
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
      RegenAndPublishIntroSet(Now(), true);
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
      auto now = Now();
      if(introset == nullptr || introset->IsExpired(now))
      {
        LogError(Name(), " failed to lookup ", addr.ToString(), " from ",
                 endpoint);
        m_ServiceLookupFails[endpoint] = m_ServiceLookupFails[endpoint] + 1;
        // inform one
        auto itr = m_PendingServiceLookups.find(addr);
        if(itr != m_PendingServiceLookups.end())
        {
          itr->second(addr, nullptr);
          m_PendingServiceLookups.erase(itr);
        }
        return false;
      }
      else
        PutNewOutboundContext(*introset);
      return true;
    }

    bool
    Endpoint::EnsurePathToService(const Address& remote, PathEnsureHook hook,
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
      {
        auto itr = m_RemoteSessions.find(remote);
        if(itr != m_RemoteSessions.end())
        {
          hook(itr->first, itr->second.get());
          return true;
        }
      }

      if(m_PendingServiceLookups.count(remote) >= MaxConcurrentLookups)
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
        m_PendingServiceLookups.emplace(remote, hook);
        return true;
      }
      LogError("send via path failed");
      return false;
    }

    void
    Endpoint::EnsurePathToSNode(const RouterID& snode, SNodeEnsureHook h)
    {
      using namespace std::placeholders;
      if(m_SNodeSessions.count(snode) == 0)
      {
        auto themIP  = ObtainIPForAddr(snode, true);
        auto session = std::make_shared< exit::SNodeSession >(
            snode,
            std::bind(&Endpoint::HandleWriteIPPacket, this, _1,
                      [themIP]() -> huint128_t { return themIP; }),
            m_Router, m_NumPaths, numHops, false, ShouldBundleRC());
        m_SNodeSessions.emplace(snode, session);
      }
      EnsureRouterIsKnown(snode);
      auto range = m_SNodeSessions.equal_range(snode);
      auto itr   = range.first;
      while(itr != range.second)
      {
        if(itr->second->IsReady())
          h(snode, itr->second);
        else
        {
          itr->second->AddReadyHook(std::bind(h, snode, _1));
          itr->second->BuildOne();
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
      EndpointLogic()->queue_func([&]() {
        // send downstream packets to user for snode
        for(const auto& item : m_SNodeSessions)
          item.second->FlushDownstream();
        // send downstream traffic to user for hidden service
        util::Lock lock(&m_InboundTrafficQueueMutex);
        while(m_InboundTrafficQueue.size())
        {
          const auto& msg = m_InboundTrafficQueue.top();
          llarp_buffer_t buf(msg->payload);
          HandleWriteIPPacket(buf, [&]() -> huint128_t {
            return ObtainIPForAddr(msg->sender.Addr(), false);
          });
          m_InboundTrafficQueue.pop();
        }
      });

      auto router = Router();
      // TODO: locking on this container
      for(const auto& item : m_RemoteSessions)
        item.second->FlushUpstream();
      // TODO: locking on this container
      for(const auto& item : m_SNodeSessions)
        item.second->FlushUpstream();
      util::Lock lock(&m_SendQueueMutex);
      // send outbound traffic
      for(const auto& item : m_SendQueue)
        item.second->SendRoutingMessage(*item.first, router);
      m_SendQueue.clear();
    }

    bool
    Endpoint::SendToServiceOrQueue(const service::Address& remote,
                                   const llarp_buffer_t& data, ProtocolType t)
    {
      // inbound converstation
      auto now = Now();

      if(HasInboundConvo(remote))
      {
        auto transfer    = std::make_shared< routing::PathTransferMessage >();
        ProtocolFrame& f = transfer->T;
        std::shared_ptr< path::Path > p;
        std::set< ConvoTag > tags;
        if(GetConvoTagsForService(remote, tags))
        {
          Introduction remoteIntro;
          SharedSecret K;
          // pick tag
          for(const auto& tag : tags)
          {
            if(tag.IsZero())
              continue;
            if(!GetCachedSessionKeyFor(tag, K))
              continue;
            if(GetIntroFor(tag, remoteIntro))
            {
              if(!remoteIntro.ExpiresSoon(now))
                p = GetNewestPathByRouter(remoteIntro.router);
              if(p)
              {
                f.T = tag;
              }
            }
          }
          if(p)
          {
            // TODO: check expiration of our end
            ProtocolMessage m(f.T);
            m.PutBuffer(data);
            f.N.Randomize();
            f.C.Zero();
            transfer->Y.Randomize();
            m.proto      = t;
            m.introReply = p->intro;
            PutReplyIntroFor(f.T, m.introReply);
            m.sender    = m_Identity.pub;
            m.seqno     = GetSeqNoForConvo(f.T);
            f.S         = 1;
            f.F         = m.introReply.pathID;
            transfer->P = remoteIntro.pathID;
            if(!f.EncryptAndSign(m, K, m_Identity))
            {
              LogError("failed to encrypt and sign");
              return false;
            }
            LogDebug(Name(), " send ", data.sz, " via ", remoteIntro.router);
            {
              util::Lock lock(&m_SendQueueMutex);
              m_SendQueue.emplace_back(transfer, p);
            }
            return true;
          }
        }
      }

      // outbound converstation
      if(EndpointUtil::HasPathToService(remote, m_RemoteSessions))
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
      m_PendingTraffic[remote].emplace_back(data, t);
      // no converstation
      return EnsurePathToService(
          remote,
          [&](Address r, OutboundContext* c) {
            if(c)
            {
              c->UpdateIntroSet(true);
              for(auto& pending : m_PendingTraffic[r])
                c->AsyncEncryptAndSendTo(pending.Buffer(), pending.protocol);
            }
            m_PendingTraffic.erase(r);
          },
          5000, true);
    }

    bool
    Endpoint::HasConvoTag(const ConvoTag& t) const
    {
      return m_Sessions.find(t) != m_Sessions.end();
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
    Endpoint::ShouldBuildMore(llarp_time_t now) const
    {
      const bool should = path::Builder::ShouldBuildMore(now);
      // determine newest intro
      Introduction intro;
      if(!GetNewestIntro(intro))
        return should;
      // time from now that the newest intro expires at
      if(intro.ExpiresSoon(now))
        return should;
      const auto dlt = intro.expiresAt - now;
      return should
          || (  // try spacing tunnel builds out evenly in time
                 (dlt <= (path::default_lifetime / 4))
                 && (NumInStatus(path::ePathBuilding) < m_NumPaths));
    }

    std::shared_ptr< Logic >
    Endpoint::RouterLogic()
    {
      return m_Router->logic();
    }

    std::shared_ptr< Logic >
    Endpoint::EndpointLogic()
    {
      return m_IsolatedLogic ? m_IsolatedLogic : m_Router->logic();
    }

    llarp_threadpool*
    Endpoint::CryptoWorker()
    {
      return m_Router->threadpool();
    }

  }  // namespace service
}  // namespace llarp
