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
#include <service/protocol.hpp>
#include <util/logic.hpp>

#include <util/buffer.hpp>

namespace llarp
{
  namespace service
  {
    Endpoint::Endpoint(const std::string& name, AbstractRouter* r,
                       Context* parent)
        : path::Builder(r, r->dht(), 3, DEFAULT_HOP_LENGTH)
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
        return m_Router->netloop();
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
        if(router->routerProfiling().IsBad(intro.router))
          continue;
        m_IntroSet.I.push_back(intro);
      }
      if(m_IntroSet.I.size() == 0)
      {
        llarp::LogWarn("not enough intros to publish introset for ", Name());
        return;
      }
      m_IntroSet.topic = m_Tag;
      if(!m_Identity.SignIntroSet(m_IntroSet, m_Router->crypto(), now))
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

      auto putContainer = [](util::StatusObject& o, const std::string& keyname,
                             const auto& container) {
        std::vector< util::StatusObject > objs;
        std::transform(container.begin(), container.end(),
                       std::back_inserter(objs),
                       [](const auto& item) -> util::StatusObject {
                         return item.second->ExtractStatus();
                       });
        o.Put(keyname, objs);
      };
      putContainer(obj, "deadSessions", m_DeadSessions);
      putContainer(obj, "remoteSessions", m_RemoteSessions);
      putContainer(obj, "snodeSessions", m_SNodeSessions);
      putContainer(obj, "lookups", m_PendingLookups);

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
      // publish descriptors
      if(ShouldPublishDescriptors(now))
      {
        RegenAndPublishIntroSet(now);
      }
      else if(NumInStatus(llarp::path::ePathEstablished) < 3)
      {
        if(m_IntroSet.HasExpiredIntros(now))
          ManualRebuild(1);
      }

      // expire snode sessions
      {
        auto itr = m_SNodeSessions.begin();
        while(itr != m_SNodeSessions.end())
        {
          if(itr->second->ShouldRemove() && itr->second->IsStopped())
          {
            itr = m_SNodeSessions.erase(itr);
            continue;
          }
          // expunge next tick
          if(itr->second->IsExpired(now))
            itr->second->Stop();

          ++itr;
        }
      }
      // expire pending tx
      {
        auto itr = m_PendingLookups.begin();
        while(itr != m_PendingLookups.end())
        {
          if(itr->second->IsTimedOut(now))
          {
            std::unique_ptr< IServiceLookup > lookup = std::move(itr->second);

            llarp::LogInfo(lookup->name, " timed out txid=", lookup->txid);
            lookup->HandleResponse({});
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
            router->routerProfiling().MarkTimeout(itr->first);
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
            llarp::LogInfo(Name(), " send message to ", introset.A.Addr(),
                           " for tag ", tag.ToString());
          else

            llarp::LogWarn(Name(), " failed to send/queue data to ",
                           introset.A.Addr(), " for tag ", tag.ToString());
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
      // tick remote sessions
      {
        auto itr = m_RemoteSessions.begin();
        while(itr != m_RemoteSessions.end())
        {
          if(itr->second->Tick(now))
          {
            itr->second->Stop();
            m_DeadSessions.emplace(itr->first, std::move(itr->second));
            itr = m_RemoteSessions.erase(itr);
          }
          else
            ++itr;
        }
      }
      // expire convotags
      {
        auto itr = m_Sessions.begin();
        while(itr != m_Sessions.end())
        {
          if(itr->second.IsExpired(now))
            itr = m_Sessions.erase(itr);
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
      (void)now;
      return AvailablePaths(path::ePathRoleAny) == 0 && ShouldRemove();
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
      // m_PendingLookups.emplace(txid, ptr);
      // m_PendingLookups[txid] = std::move(ptr);
      m_PendingLookups.emplace(txid, std::unique_ptr< IServiceLookup >(lookup));
    }

    bool
    Endpoint::HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg)
    {
      auto crypto = m_Router->crypto();
      std::set< IntroSet > remote;
      for(const auto& introset : msg->I)
      {
        if(!introset.Verify(crypto, Now()))
        {
          if(m_Identity.pub == introset.A && m_CurrentPublishTX == msg->T)
            IntroSetPublishFail();
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
        itr = m_Sessions.emplace(tag, Session{}).first;
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
        itr = m_Sessions.emplace(tag, Session{}).first;
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
        itr = m_Sessions.emplace(tag, Session{}).first;
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
        itr = m_Sessions.emplace(tag, Session{}).first;
      }
      itr->second.sharedKey = k;
      itr->second.lastUsed  = Now();
    }

    bool
    Endpoint::LoadKeyFile()
    {
      auto crypto = m_Router->crypto();
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
      auto now = Now();
      if(ShouldPublishDescriptors(now))
      {
        RegenAndPublishIntroSet(now);
      }
      else if(NumInStatus(llarp::path::ePathEstablished) < 3)
      {
        if(m_IntroSet.HasExpiredIntros(now))
          ManualRebuild(1);
      }
    }

    bool
    Endpoint::PublishIntroSetVia(AbstractRouter* r, path::Path* path)
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
          IntroSet selected;
          for(const auto& introset : results)
          {
            if(selected.OtherIsNewer(introset) && introset.A.Addr() == remote)
              selected = introset;
          }
          return handle(remote, &selected, endpoint);
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

      OutboundContext* ctx = new OutboundContext(introset, this);
      m_RemoteSessions.emplace(addr, std::unique_ptr< OutboundContext >(ctx));
      llarp::LogInfo("Created New outbound context for ", addr.ToString());

      // inform pending
      auto range = m_PendingServiceLookups.equal_range(addr);
      auto itr   = range.first;
      if(itr != range.second)
      {
        itr->second(addr, ctx);
        ++itr;
      }
      m_PendingServiceLookups.erase(addr);
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
        job->nodedb                = m_Router->nodedb();
        job->cryptoworker          = m_Router->threadpool();
        job->diskworker            = m_Router->diskworker();
        job->logic                 = m_Router->logic();
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
      if(!m_Router->nodedb()->Get(router, rc))
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
          m_PendingRouters.emplace(router, RouterLookupJob(this));
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
    Endpoint::HandleDataMessage(const PathID_t& src, ProtocolMessage* msg)
    {
      auto path = GetPathByID(src);
      if(path)
        PutReplyIntroFor(msg->tag, path->intro);
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
        llarp_buffer_t buf(msg->payload);
        return HandleWriteIPPacket(buf,
                                   std::bind(&Endpoint::ObtainIPForAddr, this,
                                             msg->sender.Addr(), false));
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
    Endpoint::HandleHiddenServiceFrame(path::Path* p,
                                       const ProtocolFrame* frame)
    {
      if(frame->R)
      {
        // handle discard
        ServiceInfo si;
        if(!GetSenderFor(frame->T, si))
          return false;
        // verify source
        if(!frame->Verify(Crypto(), si))
          return false;
        // remove convotag it doesn't exist
        LogWarn("remove convotag T=", frame->T);
        RemoveConvoTag(frame->T);
        return true;
      }
      if(!frame->AsyncDecryptAndVerify(EndpointLogic(), Crypto(), p->RXID(),
                                       Worker(), m_Identity, m_DataHandler))
      {
        // send discard
        ProtocolFrame f;
        f.R = 1;
        f.T = frame->T;
        f.F = p->intro.pathID;
        if(!f.Sign(Crypto(), m_Identity))
          return false;
        const routing::PathTransferMessage d(f, frame->F);
        return p->SendRoutingMessage(&d, router);
      }
      return true;
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
      currentConvoTag.Zero();
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
    }

    void
    Endpoint::HandlePathDead(void* user)
    {
      Endpoint* self = static_cast< Endpoint* >(user);
      self->RegenAndPublishIntroSet(self->Now(), true);
    }

    bool
    Endpoint::CheckPathIsDead(path::Path*, llarp_time_t)
    {
      RouterLogic()->call_later(
          {100, this, [](void* u, uint64_t, uint64_t left) {
             if(left)
               return;
             HandlePathDead(u);
           }});
      return true;
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

      if(m_PendingServiceLookups.count(remote) >= MaxConcurrentLookups)
      {
        llarp::LogWarn(Name(), " has too many pending service lookups for ",
                       remote.ToString());
        return false;
      }

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
      llarp::LogInfo("doing lookup for ", remote, " via ", path->Endpoint());
      if(job->SendRequestViaPath(path, Router()))
      {
        m_PendingServiceLookups.emplace(remote, hook);
        return true;
      }
      llarp::LogError("send via path failed");
      return false;
    }

    Endpoint::OutboundContext::OutboundContext(const IntroSet& introset,
                                               Endpoint* parent)
        : path::Builder(parent->m_Router, parent->m_Router->dht(), 3,
                        DEFAULT_HOP_LENGTH)
        , SendContext(introset.A, {}, this, parent)
        , currentIntroSet(introset)

    {
      auto& profiling  = parent->m_Router->routerProfiling();
      updatingIntroSet = false;
      for(const auto intro : introset.I)
      {
        if(intro.expiresAt > m_NextIntro.expiresAt
           && !profiling.IsBad(intro.router))
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
      m_DataHandler->PutIntroFor(currentConvoTag, remoteIntro);
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
      else
        ++m_LookupFails;
      return true;
    }

    bool
    Endpoint::OutboundContext::ReadyToSend() const
    {
      return (!remoteIntro.router.IsZero())
          && GetPathByRouter(remoteIntro.router) != nullptr;
    }

    void
    Endpoint::EnsurePathToSNode(const RouterID& snode, SNodeEnsureHook h)
    {
      if(m_SNodeSessions.count(snode) == 0)
      {
        auto themIP = ObtainIPForAddr(snode, true);
        m_SNodeSessions.emplace(
            snode,
            std::make_unique< exit::SNodeSession >(
                snode,
                std::bind(&Endpoint::HandleWriteIPPacket, this,
                          std::placeholders::_1,
                          [themIP]() -> huint32_t { return themIP; }),
                m_Router, 2, numHops));
      }
      auto range = m_SNodeSessions.equal_range(snode);
      auto itr   = range.first;
      while(itr != range.second)
      {
        if(itr->second->IsReady())
          h(snode, itr->second.get());
        else
          itr->second->AddReadyHook(std::bind(h, snode, std::placeholders::_1));
        ++itr;
      }
    }

    bool
    Endpoint::SendToSNodeOrQueue(const RouterID& addr,
                                 const llarp_buffer_t& buf)
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
          if(GetConvoTagsForService(itr->second, tags))
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
              if(p == nullptr && GetIntroFor(tag, remoteIntro))
              {
                if(!remoteIntro.ExpiresSoon(now))
                  p = GetNewestPathByRouter(remoteIntro.router);
                if(p)
                {
                  f.T = tag;
                  break;
                }
              }
            }
            if(p)
            {
              // TODO: check expiration of our end
              ProtocolMessage m(f.T);
              PutReplyIntroFor(f.T, m.introReply);
              m.PutBuffer(data);
              f.N.Randomize();
              f.C.Zero();
              transfer.Y.Randomize();
              m.proto      = t;
              m.introReply = p->intro;
              m.sender     = m_Identity.pub;
              f.F          = m.introReply.pathID;
              f.S          = GetSeqNoForConvo(f.T);
              transfer.P   = remoteIntro.pathID;
              if(!f.EncryptAndSign(Router()->crypto(), m, K, m_Identity))
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
      return EnsurePathToService(remote,
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
      auto nodedb = m_Endpoint->Router()->nodedb();
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
          } while(
              m_Endpoint->Router()->routerProfiling().IsBad(hops[hop].pubkey)
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
        if(router->routerProfiling().IsBad(intro.router))
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
        self->handler->PutReplyIntroFor(self->msg.tag, self->msg.introReply);
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
        std::array< byte_t, 64 > tmp = {{0}};
        // K
        std::copy(K.begin(), K.end(), tmp.begin());
        // H (K + PKE(A, B, N))
        std::copy(sharedSecret.begin(), sharedSecret.end(), tmp.begin() + 32);
        self->crypto->shorthash(self->sharedKey, llarp_buffer_t(tmp));
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
          BuildOneAlignedTo(remoteIntro.router);
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
      ex->frame.F        = ex->msg.introReply.pathID;
      llarp_threadpool_queue_job(m_Endpoint->Worker(),
                                 {ex, &AsyncKeyExchange::Encrypt});
    }

    bool
    Endpoint::SendContext::Send(const ProtocolFrame& msg)
    {
      auto path = m_PathSet->GetByEndpointWithID(remoteIntro.router, msg.F);
      if(path)
      {
        const routing::PathTransferMessage transfer(msg, remoteIntro.pathID);
        if(path->SendRoutingMessage(&transfer, m_Endpoint->Router()))
        {
          llarp::LogInfo("sent intro to ", remoteIntro.pathID, " on ",
                         remoteIntro.router, " seqno=", sequenceNo);
          lastGoodSend = m_Endpoint->Now();
          ++sequenceNo;
          return true;
        }
        else
          llarp::LogError("Failed to send frame on path");
      }
      else
        llarp::LogError("cannot send because we have no path to ",
                        remoteIntro.router);
      return false;
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

    util::StatusObject
    Endpoint::OutboundContext::ExtractStatus() const
    {
      auto obj = path::Builder::ExtractStatus();
      obj.Put("currentConvoTag", currentConvoTag.ToHex());
      obj.Put("remoteIntro", remoteIntro.ExtractStatus());
      obj.Put("sessionCreatedAt", createdAt);
      obj.Put("lastGoodSend", lastGoodSend);
      obj.Put("seqno", sequenceNo);
      obj.Put("markedBad", markedBad);
      obj.Put("lastShift", lastShift);
      obj.Put("remoteIdentity", remoteIdent.Addr().ToString());
      obj.Put("currentRemoteIntroset", currentIntroSet.ExtractStatus());
      obj.Put("nextIntro", m_NextIntro.ExtractStatus());
      std::vector< util::StatusObject > badIntrosObj;
      std::transform(m_BadIntros.begin(), m_BadIntros.end(),
                     std::back_inserter(badIntrosObj),
                     [](const auto& item) -> util::StatusObject {
                       util::StatusObject o{
                           {"count", item.second},
                           {"intro", item.first.ExtractStatus()}};
                       return o;
                     });
      obj.Put("badIntros", badIntrosObj);
      return obj;
    }

    bool
    Endpoint::OutboundContext::Tick(llarp_time_t now)
    {
      // we are probably dead af
      if(m_LookupFails > 16)
        return true;
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
          llarp::LogInfo(Name(), " swapped intro");
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
      // send control message if we look too quiet
      if(lastGoodSend)
      {
        if(now - lastGoodSend > (sendTimeout / 2))
        {
          if(!GetNewestPathByRouter(remoteIntro.router))
          {
            BuildOneAlignedTo(remoteIntro.router);
          }
          Encrypted< 64 > tmp;
          tmp.Randomize();
          llarp_buffer_t buf(tmp.data(), tmp.size());
          AsyncEncryptAndSendTo(buf, eProtocolControl);
          SharedSecret k;
          if(currentConvoTag.IsZero())
            return false;
          return !m_DataHandler->HasConvoTag(currentConvoTag);
        }
      }
      // if we are dead return true so we are removed
      return lastGoodSend
          ? (now >= lastGoodSend && now - lastGoodSend > sendTimeout)
          : (now >= createdAt && now - createdAt > connectTimeout);
    }

    bool
    Endpoint::HasConvoTag(const ConvoTag& t) const
    {
      return m_Sessions.find(t) != m_Sessions.end();
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
      else if(hop == numHops - 2)
      {
        return db->select_random_hop_excluding(
            cur, {prev.pubkey, m_NextIntro.router});
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
      if(path::Builder::ShouldBuildMore(now))
        return true;
      return !ReadyToSend();
    }

    bool
    Endpoint::ShouldBuildMore(llarp_time_t now) const
    {
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
      auto crypto = m_Endpoint->Router()->crypto();
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
        m_DataHandler->PutIntroFor(f.T, remoteIntro);
        m_DataHandler->PutReplyIntroFor(f.T, path->intro);
        m.proto      = t;
        m.introReply = path->intro;
        f.F          = m.introReply.pathID;
        m.sender     = m_Endpoint->m_Identity.pub;
        m.tag        = f.T;
        m.PutBuffer(payload);
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
      if(path->SendRoutingMessage(&msg, m_Endpoint->Router()))
      {
        llarp::LogDebug("sent message via ", remoteIntro.pathID, " on ",
                        remoteIntro.router);
        ++sequenceNo;
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
      return m_Router->logic();
    }

    llarp::Logic*
    Endpoint::EndpointLogic()
    {
      return m_IsolatedLogic ? m_IsolatedLogic : m_Router->logic();
    }

    llarp::Crypto*
    Endpoint::Crypto()
    {
      return m_Router->crypto();
    }

    llarp_threadpool*
    Endpoint::Worker()
    {
      return m_Router->threadpool();
    }

  }  // namespace service
}  // namespace llarp
