
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
        : llarp_pathbuilder_context(r, r->dht, 2, 4), m_Router(r), m_Name(name)
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
      return true;
    }

    bool
    Endpoint::IsolateNetwork()
    {
      m_IsolatedWorker = llarp_init_isolated_net_threadpool(
          m_Name.c_str(), &SetupIsolatedNetwork, this);
      m_IsolatedLogic = llarp_init_single_process_logic(m_IsolatedWorker);
      return true;
    }

    struct PathAlignJob
    {
      Address remote;

      PathAlignJob(const Address& addr) : remote(addr)
      {
      }

      void
      HandleResult(Endpoint::OutboundContext* context)
      {
        if(context)
        {
          llarp::LogInfo("BEEP");
          byte_t tmp[128] = {0};
          memcpy(tmp, "BEEP", 4);
          auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
          context->AsyncEncryptAndSendTo(buf, eProtocolText);
        }
        else
        {
          llarp::LogWarn("PathAlignJob timed out");
        }
        delete this;
      }
    };

    bool
    Endpoint::SetupIsolatedNetwork(void* user)
    {
      return static_cast< Endpoint* >(user)->DoNetworkIsolation();
    }

    bool
    Endpoint::HasPendingPathToService(const Address& addr) const
    {
      return m_PendingServiceLookups.find(addr)
          != m_PendingServiceLookups.end();
    }

    void
    Endpoint::Tick(llarp_time_t now)
    {
      /// reset tx id for publish
      if(now - m_LastPublishAttempt >= INTROSET_PUBLISH_RETRY_INTERVAL)
        m_CurrentPublishTX = 0;
      // publish descriptors
      if(ShouldPublishDescriptors(now))
      {
        std::set< Introduction > I;
        if(!GetCurrentIntroductions(I))
        {
          llarp::LogWarn("could not publish descriptors for endpoint ", Name(),
                         " because we couldn't get any introductions");
          return;
        }
        m_IntroSet.I.clear();
        for(const auto& intro : I)
          m_IntroSet.I.push_back(intro);
        m_IntroSet.topic = m_Tag;
        if(!m_Identity.SignIntroSet(m_IntroSet, &m_Router->crypto))
        {
          llarp::LogWarn("failed to sign introset for endpoint ", Name());
          return;
        }
        if(PublishIntroSet(m_Router))
        {
          llarp::LogInfo("publishing introset for endpoint ", Name());
        }
        else
        {
          llarp::LogWarn("failed to publish intro set for endpoint ", Name());
        }
      }
      // expire pending tx
      {
        auto itr = m_PendingLookups.begin();
        while(itr != m_PendingLookups.end())
        {
          if(itr->second->IsTimedOut(now))
          {
            itr->second->HandleResponse({});
            itr = m_PendingLookups.erase(itr);
          }
          else
            ++itr;
        }
      }

      for(const auto& addr : m_PrefetchAddrs)
      {
        if(!HasPathToService(addr))
        {
          PathAlignJob* j = new PathAlignJob(addr);
          if(!EnsurePathToService(j->remote,
                                  std::bind(&PathAlignJob::HandleResult, j,
                                            std::placeholders::_1),
                                  10000))
          {
            llarp::LogWarn("failed to ensure path to ", addr);
            delete j;
          }
        }
      }

      // prefetch tags
      for(const auto& tag : m_PrefetchTags)
      {
        auto itr = m_PrefetchedTags.find(tag);
        if(itr == m_PrefetchedTags.end())
        {
          itr = m_PrefetchedTags
                    .insert(std::make_pair(
                        tag, CachedTagResult(this, tag, GenTXID())))
                    .first;
        }
        for(const auto& introset : itr->second.result)
        {
          if(HasPendingPathToService(introset.A.Addr()))
            continue;
          PathAlignJob* j = new PathAlignJob(introset.A.Addr());
          if(!EnsurePathToService(j->remote,
                                  std::bind(&PathAlignJob::HandleResult, j,
                                            std::placeholders::_1),
                                  10000))
          {
            llarp::LogWarn("failed to ensure path to ", introset.A.Addr(),
                           " for tag ", tag.ToString());
            delete j;
          }
        }
        itr->second.Expire(now);
        if(itr->second.ShouldRefresh(now))
        {
          auto path = PickRandomEstablishedPath();
          if(path)
          {
            itr->second.txid = GenTXID();
            itr->second.SendRequestViaPath(path, m_Router);
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
            delete itr->second;
            itr = m_RemoteSessions.erase(itr);
          }
          else
            ++itr;
        }
      }
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
      m_PendingLookups.insert(std::make_pair(txid, lookup));
    }

    bool
    Endpoint::HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg)
    {
      auto crypto = &m_Router->crypto;
      std::set< IntroSet > remote;
      for(const auto& introset : msg->I)
      {
        if(!introset.VerifySignature(crypto))
        {
          llarp::LogInfo("invalid introset signature for ", introset,
                         " on endpoint ", Name());
          if(m_Identity.pub == introset.A && m_CurrentPublishTX == msg->T)
          {
            IntroSetPublishFail();
          }
          return false;
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
      bool result = itr->second->HandleResponse(remote);
      m_PendingLookups.erase(itr);
      return result;
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

    Endpoint::CachedTagResult::~CachedTagResult()
    {
    }

    bool
    Endpoint::CachedTagResult::HandleResponse(
        const std::set< IntroSet >& introsets)
    {
      auto now = llarp_time_now_ms();

      txid = 0;
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
    Endpoint::CachedTagResult::BuildRequestMessage()
    {
      llarp::routing::DHTMessage* msg = new llarp::routing::DHTMessage();
      msg->M.push_back(new llarp::dht::FindIntroMessage({}, tag, txid));
      lastRequest = llarp_time_now_ms();
      parent->PutLookup(this, txid);
      return msg;
    }

    bool
    Endpoint::PublishIntroSet(llarp_router* r)
    {
      auto path = GetEstablishedPathClosestTo(m_Identity.pub.Addr());
      if(path)
      {
        m_CurrentPublishTX = llarp_randint();
        llarp::routing::DHTMessage msg;
        msg.M.push_back(new llarp::dht::PublishIntroMessage(
            m_IntroSet, m_CurrentPublishTX, 4));
        if(path->SendRoutingMessage(&msg, r))
        {
          m_LastPublishAttempt = llarp_time_now_ms();
          llarp::LogInfo(Name(), " publishing introset");
          return true;
        }
      }
      llarp::LogWarn(Name(), " publish introset failed, no path");
      return false;
    }

    void
    Endpoint::IntroSetPublishFail()
    {
      llarp::LogWarn("failed to publish introset for ", Name());
      m_CurrentPublishTX = 0;
    }

    bool
    Endpoint::ShouldPublishDescriptors(llarp_time_t now) const
    {
      if(m_IntroSet.HasExpiredIntros(now))
        return m_CurrentPublishTX == 0
            && now - m_LastPublishAttempt >= INTROSET_PUBLISH_RETRY_INTERVAL;
      return m_CurrentPublishTX == 0
          && now - m_LastPublish >= INTROSET_PUBLISH_INTERVAL;
    }

    void
    Endpoint::IntroSetPublished()
    {
      m_CurrentPublishTX = 0;
      m_LastPublish      = llarp_time_now_ms();
      llarp::LogInfo(Name(), " IntroSet publish confirmed");
    }

    struct HiddenServiceAddressLookup : public IServiceLookup
    {
      Address remote;
      Endpoint* endpoint;

      HiddenServiceAddressLookup(Endpoint* p, const Address& addr, uint64_t tx)
          : IServiceLookup(p, tx), remote(addr), endpoint(p)
      {
        llarp::LogInfo("New hidden service lookup for ", addr.ToString());
      }

      bool
      HandleResponse(const std::set< IntroSet >& results)
      {
        llarp::LogInfo("found ", results.size(), " for ", remote.ToString());
        if(results.size() == 1)
        {
          llarp::LogInfo("hidden service lookup for ", remote.ToString(),
                         " success");
          endpoint->PutNewOutboundContext(*results.begin());
        }
        else
        {
          llarp::LogInfo("no response in hidden service lookup for ",
                         remote.ToString());
        }
        delete this;
        return true;
      }

      llarp::routing::IMessage*
      BuildRequestMessage()
      {
        llarp::routing::DHTMessage* msg = new llarp::routing::DHTMessage();
        msg->M.push_back(new llarp::dht::FindIntroMessage(remote, txid));
        return msg;
      }
    };

    bool
    Endpoint::DoNetworkIsolation()
    {
      /// TODO: implement me
      return false;
    }

    void
    Endpoint::PutNewOutboundContext(const llarp::service::IntroSet& introset)
    {
      Address addr;
      introset.A.CalculateAddress(addr);

      // only add new session if it's not there
      if(m_RemoteSessions.find(addr) == m_RemoteSessions.end())
      {
        OutboundContext* ctx = new OutboundContext(introset, this);
        m_RemoteSessions.insert(std::make_pair(addr, ctx));
        llarp::LogInfo("Created New outbound context for ", addr.ToString());
      }

      // inform pending
      auto itr = m_PendingServiceLookups.find(addr);
      if(itr != m_PendingServiceLookups.end())
      {
        itr->second(m_RemoteSessions.at(addr));
        m_PendingServiceLookups.erase(itr);
      }
    }

    void
    Endpoint::HandlePathBuilt(path::Path* p)
    {
      p->SetDataHandler(std::bind(&Endpoint::HandleHiddenServiceFrame, this,
                                  std::placeholders::_1));
    }

    bool
    Endpoint::HandleHiddenServiceFrame(const ProtocolFrame* frame)
    {
      return frame->AsyncDecryptAndVerify(EndpointLogic(), Crypto(), Worker(),
                                          m_Identity.enckey, m_DataHandler);
    }

    void
    Endpoint::OutboundContext::HandlePathBuilt(path::Path* p)
    {
      p->SetDataHandler(
          std::bind(&Endpoint::OutboundContext::HandleHiddenServiceFrame, this,
                    std::placeholders::_1));
    }

    void
    Endpoint::OutboundContext::PutLookup(IServiceLookup* lookup, uint64_t txid)
    {
      m_Parent->PutLookup(lookup, txid);
    }

    bool
    Endpoint::OutboundContext::HandleHiddenServiceFrame(
        const ProtocolFrame* frame)
    {
      return m_Parent->HandleHiddenServiceFrame(frame);
    }

    bool
    Endpoint::EnsurePathToService(const Address& remote, PathEnsureHook hook,
                                  llarp_time_t timeoutMS)
    {
      auto path = GetEstablishedPathClosestTo(remote);
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
          hook(itr->second);
          return true;
        }
      }
      auto itr = m_PendingServiceLookups.find(remote);
      if(itr != m_PendingServiceLookups.end())
      {
        // duplicate
        llarp::LogWarn("duplicate pending service lookup to ",
                       remote.ToString());
        return false;
      }

      m_PendingServiceLookups.insert(std::make_pair(remote, hook));

      HiddenServiceAddressLookup* job =
          new HiddenServiceAddressLookup(this, remote, GenTXID());

      return job->SendRequestViaPath(path, Router());
    }

    Endpoint::OutboundContext::OutboundContext(const IntroSet& intro,
                                               Endpoint* parent)
        : llarp_pathbuilder_context(parent->m_Router, parent->m_Router->dht, 2,
                                    4)
        , currentIntroSet(intro)
        , m_Parent(parent)

    {
      selectedIntro.Clear();
      ShiftIntroduction();
    }

    Endpoint::OutboundContext::~OutboundContext()
    {
    }

    void
    Endpoint::OutboundContext::ShiftIntroduction()
    {
      for(const auto& intro : currentIntroSet.I)
      {
        if(intro.expiresAt > selectedIntro.expiresAt)
        {
          selectedIntro = intro;
        }
      }
    }

    bool
    Endpoint::OutboundContext::HandleGotIntroMessage(
        const llarp::dht::GotIntroMessage* msg)
    {
      if(msg->T != m_UpdateIntrosetTX)
      {
        llarp::LogError("unwarrented introset message txid=", msg->T);
        return false;
      }
      auto crypto = m_Parent->Crypto();
      if(msg->I.size() == 1)
      {
        // found intro set
        const auto& introset = msg->I[0];
        if(introset.VerifySignature(crypto) && currentIntroSet.A == introset.A)
        {
          // update
          currentIntroSet = introset;
          // reset tx
          m_UpdateIntrosetTX = 0;
          // shift to newest intro
          // TODO: check timestamp on introset to make sure it's new enough
          ShiftIntroduction();
          return true;
        }
        else
        {
          llarp::LogError("Signature Error for intro set ", introset);
          return false;
        }
      }
      llarp::LogError("Bad number of intro sets in response");
      return false;
    }

    void
    Endpoint::OutboundContext::AsyncEncryptAndSendTo(llarp_buffer_t data,
                                                     ProtocolType protocol)
    {
      if(sequenceNo)
      {
        EncryptAndSendTo(data);
      }
      else
      {
        AsyncGenIntro(data);
      }
    }

    struct AsyncIntroGen
    {
      llarp_logic* logic;
      llarp_crypto* crypto;
      byte_t* sharedKey;
      ServiceInfo remote;
      Identity* m_LocalIdentity;
      ProtocolMessage msg;
      ProtocolFrame frame;
      Introduction intro;
      std::function< void(ProtocolFrame&) > hook;
      IDataHandler* handler;

      AsyncIntroGen(llarp_logic* l, llarp_crypto* c, byte_t* key,
                    const ServiceInfo& r, Identity* localident,
                    const Introduction& us, IDataHandler* h)
          : logic(l)
          , crypto(c)
          , sharedKey(key)
          , remote(r)
          , m_LocalIdentity(localident)
          , intro(us)
          , handler(h)
      {
      }

      static void
      Result(void* user)
      {
        AsyncIntroGen* self = static_cast< AsyncIntroGen* >(user);
        // put values
        self->handler->PutCachedSessionKeyFor(self->msg.tag, self->sharedKey);
        self->handler->PutIntroFor(self->msg.tag, self->msg.introReply);
        self->handler->PutSenderFor(self->msg.tag, self->remote);
        self->hook(self->frame);
        delete self;
      }

      static void
      Work(void* user)
      {
        AsyncIntroGen* self = static_cast< AsyncIntroGen* >(user);
        // randomize Nounce
        self->frame.N.Randomize();
        // randomize tag
        self->msg.tag.Randomize();
        // set sender
        self->msg.sender = self->m_LocalIdentity->pub;
        // set our introduction
        self->msg.introReply = self->intro;
        // derive session key
        self->crypto->dh_server(self->sharedKey, self->remote.enckey,
                                self->m_LocalIdentity->enckey, self->frame.N);

        // encrypt and sign
        self->frame.EncryptAndSign(self->crypto, &self->msg, self->sharedKey,
                                   self->m_LocalIdentity->signkey);
        // inform result
        llarp_logic_queue_job(self->logic, {self, &Result});
      }
    };

    void
    Endpoint::OutboundContext::AsyncGenIntro(llarp_buffer_t payload)
    {
      AsyncIntroGen* ex = new AsyncIntroGen(
          m_Parent->RouterLogic(), m_Parent->Crypto(), sharedKey,
          currentIntroSet.A, m_Parent->GetIdentity(), selectedIntro,
          m_Parent->m_DataHandler);
      ex->hook = std::bind(&Endpoint::OutboundContext::Send, this,
                           std::placeholders::_1);

      ex->msg.PutBuffer(payload);
      llarp_threadpool_queue_job(m_Parent->Worker(),
                                 {ex, &AsyncIntroGen::Work});
    }

    void
    Endpoint::OutboundContext::Send(ProtocolFrame& msg)
    {
      // in this context we assume the message contents are encrypted
      auto now = llarp_time_now_ms();
      if(currentIntroSet.HasExpiredIntros(now))
      {
        UpdateIntroSet();
      }
      if(selectedIntro.expiresAt <= now || now - selectedIntro.expiresAt > 1000)
      {
        ShiftIntroduction();
      }
      auto path = GetPathByRouter(selectedIntro.router);
      if(path)
      {
        routing::PathTransferMessage transfer;
        transfer.T = &msg;
        transfer.Y.Randomize();
        transfer.P = selectedIntro.pathID;
        llarp::LogInfo("sending frame via ", path->Upstream(), " to ",
                       path->Endpoint(), " for ", Name());
        path->SendRoutingMessage(&transfer, m_Parent->Router());
      }
      else
      {
        llarp::LogWarn("No path to ", selectedIntro.router);
      }
    }

    std::string
    Endpoint::OutboundContext::Name() const
    {
      return "OBContext:" + m_Parent->Name() + "-"
          + currentIntroSet.A.Addr().ToString();
    }

    void
    Endpoint::OutboundContext::UpdateIntroSet()
    {
      auto path = GetEstablishedPathClosestTo(currentIntroSet.A.Addr());
      if(path)
      {
        if(m_UpdateIntrosetTX == 0)
        {
          m_UpdateIntrosetTX = llarp_randint();
          routing::DHTMessage msg;
          msg.M.push_back(new llarp::dht::FindIntroMessage(
              currentIntroSet.A.Addr(), m_UpdateIntrosetTX));
          path->SendRoutingMessage(&msg, m_Parent->Router());
        }
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
      if(selectedIntro.expiresAt >= now
         || selectedIntro.expiresAt - now < 30000)
      {
        UpdateIntroSet();
      }
      // TODO: check for expiration
      return false;
    }

    bool
    Endpoint::OutboundContext::SelectHop(llarp_nodedb* db, llarp_rc* prev,
                                         llarp_rc* cur, size_t hop)
    {
      // TODO: don't hard code
      llarp::LogInfo("Select hop ", hop);
      if(hop == 3)
      {
        auto localcopy = llarp_nodedb_get_rc(db, selectedIntro.router);
        if(localcopy)
        {
          llarp_rc_copy(cur, localcopy);
          return true;
        }
        else
        {
          // we don't have it?
          llarp::LogError(
              "cannot build aligned path, don't have router for "
              "introduction ",
              selectedIntro);
          return false;
        }
      }
      else
        return llarp_pathbuilder_context::SelectHop(db, prev, cur, hop);
    }

    uint64_t
    Endpoint::GetSeqNoForConvo(const ConvoTag& tag)
    {
      auto itr = m_Sessions.find(tag);
      if(itr == m_Sessions.end())
        return 0;
      return ++(itr->second.seqno);
    }

    void
    Endpoint::OutboundContext::EncryptAndSendTo(llarp_buffer_t payload)
    {
      auto path = GetPathByRouter(selectedIntro.router);
      if(path)
      {
        std::set< ConvoTag > tags;
        if(!m_Parent->m_DataHandler->GetConvoTagsForService(currentIntroSet.A,
                                                            tags))
        {
          llarp::LogError("no open converstations with remote endpoint?");
          return;
        }
        auto crypto = m_Parent->Crypto();
        SharedSecret shared;

        ProtocolFrame f;
        f.N.Randomize();
        f.T = *tags.begin();
        f.S = m_Parent->GetSeqNoForConvo(f.T);

        if(m_Parent->m_DataHandler->GetCachedSessionKeyFor(f.T, shared))
        {
          ProtocolMessage msg;
          msg.introReply = selectedIntro;
          msg.sender     = m_Parent->m_Identity.pub;
          msg.PutBuffer(payload);

          if(!f.EncryptAndSign(crypto, &msg, shared, currentIntroSet.A.signkey))
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

        routing::PathTransferMessage msg;
        msg.P = selectedIntro.pathID;
        msg.Y.Randomize();
        msg.T = &f;
        if(!path->SendRoutingMessage(&msg, m_Parent->Router()))
        {
          llarp::LogWarn("Failed to send routing message for data");
        }
      }
      else
      {
        llarp::LogError("no outbound path for sending message");
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
