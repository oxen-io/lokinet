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
                    .insert(std::make_pair(tag, CachedTagResult(tag, now)))
                    .first;
        }
        for(const auto& introset : itr->second.result)
        {
          PathAlignJob* j = new PathAlignJob(introset.A.Addr());
          if(!EnsurePathToService(j->remote,
                                  std::bind(&PathAlignJob::HandleResult, j,
                                            std::placeholders::_1),
                                  10000))
          {
            llarp::LogWarn("failed to ensure path to ", introset.A.Addr(),
                           " for tag");
            delete j;
          }
        }
        itr->second.Expire(now);
        if(itr->second.ShouldRefresh(now))
        {
          auto path = PickRandomEstablishedPath();
          if(path)
          {
            itr->second.pendingTX                   = GenTXID();
            m_PendingLookups[itr->second.pendingTX] = &itr->second;
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

      pendingTX = 0;
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
      msg->M.push_back(new llarp::dht::FindIntroMessage(tag, pendingTX));
      lastRequest = llarp_time_now_ms();
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
            m_IntroSet, m_CurrentPublishTX, 3));
        if(path->SendRoutingMessage(&msg, r))
        {
          m_LastPublishAttempt = llarp_time_now_ms();
          llarp::LogInfo(Name(), " publishing introset");
          return true;
        }
      }
      llarp::LogWarn(Name(), " publish introset failed");
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
      Endpoint* endpoint;
      Address remote;
      uint64_t txid;
      HiddenServiceAddressLookup(Endpoint* parent, const Address& addr,
                                 uint64_t tx)
          : endpoint(parent), remote(addr), txid(tx)
      {
        llarp::LogInfo("New hidden service lookup for ", addr.ToString());
      }

      bool
      HandleResponse(const std::set< IntroSet >& results)
      {
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
      llarp::LogInfo("handle hidden service frame");
      return true;
    }

    void
    Endpoint::OutboundContext::HandlePathBuilt(path::Path* p)
    {
      p->SetDataHandler(
          std::bind(&Endpoint::OutboundContext::HandleHiddenServiceFrame, this,
                    std::placeholders::_1));
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
      m_PendingLookups.insert(std::make_pair(job->txid, job));

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
      auto crypto = m_Parent->Crypto();
      if(msg->I.size() == 1)
      {
        // found intro set
        auto itr = msg->I.begin();
        if(itr->VerifySignature(crypto) && currentIntroSet.A == itr->A)
        {
          currentIntroSet = *itr;
          ShiftIntroduction();
          return true;
        }
        else
        {
          llarp::LogError("Signature Error for intro set ", *itr);
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
        AsyncEncrypt(data);
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
      byte_t* remotePubkey;
      Identity* m_LocalIdentity;
      ProtocolMessage msg;
      ProtocolFrame frame;
      std::function< void(ProtocolFrame&) > hook;

      AsyncIntroGen(llarp_logic* l, llarp_crypto* c, byte_t* key,
                    byte_t* remote, Identity* localident)
          : logic(l)
          , crypto(c)
          , sharedKey(key)
          , remotePubkey(remote)
          , m_LocalIdentity(localident)
      {
      }

      static void
      Result(void* user)
      {
        AsyncIntroGen* self = static_cast< AsyncIntroGen* >(user);
        self->hook(self->frame);
        delete self;
      }

      static void
      Work(void* user)
      {
        AsyncIntroGen* self = static_cast< AsyncIntroGen* >(user);
        // randomize Nounce
        self->frame.N.Randomize();
        // derive session key
        self->crypto->dh_server(self->sharedKey, self->remotePubkey,
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
      AsyncIntroGen* ex =
          new AsyncIntroGen(m_Parent->Logic(), m_Parent->Crypto(), sharedKey,
                            currentIntroSet.A.enckey, m_Parent->GetIdentity());
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
                       path->Endpoint());
        path->SendRoutingMessage(&transfer, m_Parent->Router());
      }
      else
      {
        llarp::LogWarn("No path to ", selectedIntro.router);
      }
    }

    void
    Endpoint::OutboundContext::UpdateIntroSet()
    {
      auto path = PickRandomEstablishedPath();
      if(path)
      {
        uint64_t txid = llarp_randint();
        routing::DHTMessage msg;
        msg.M.push_back(
            new llarp::dht::FindIntroMessage(currentIntroSet.A.Addr(), txid));
        path->SendRoutingMessage(&msg, m_Parent->Router());
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
      if(selectedIntro.expiresAt >= now || selectedIntro.expiresAt - now < 5000)
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
              "cannot build aligned path, don't have router for introduction ",
              selectedIntro);
          return false;
        }
      }
      else
        return llarp_pathbuilder_context::SelectHop(db, prev, cur, hop);
    }

    void
    Endpoint::OutboundContext::AsyncEncrypt(llarp_buffer_t payload)
    {
      // TODO: implement me
    }

    llarp_logic*
    Endpoint::Logic()
    {
      return m_Router->logic;
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
