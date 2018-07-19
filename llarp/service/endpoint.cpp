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
        m_IntroSet.I     = I;
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
            delete j;
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
    }

    uint64_t
    Endpoint::GenTXID()
    {
      uint64_t txid = rand();
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
        return false;
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
      auto path = PickRandomEstablishedPath();
      if(path)
      {
        m_CurrentPublishTX = rand();
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
      HiddenServiceAddressLookup(Endpoint* parent) : endpoint(parent)
      {
      }

      bool
      HandleResponse(const std::set< IntroSet >& results)
      {
        if(results.size() == 0)
        {
          auto itr = results.begin();
          endpoint->PutNewOutboundContext(*itr);
        }
        else
        {
          // TODO: retry request?
        }
        return true;
      }
    };

    bool
    Endpoint::EnsurePathToService(const Address& remote, PathEnsureHook hook,
                                  llarp_time_t timeoutMS)
    {
      // TODO: implement me
      return false;
    }

    Endpoint::OutboundContext::OutboundContext(const IntroSet& intro,
                                               Endpoint* parent)
        : llarp_pathbuilder_context(parent->m_Router, parent->m_Router->dht, 2,
                                    4)
        , currentIntroSet(intro)
        , m_SendQueue(parent->Name() + "::outbound_queue")
        , m_Parent(parent)

    {
    }

    Endpoint::OutboundContext::~OutboundContext()
    {
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
      auto sendto =
          std::bind(&OutboundContext::SendMessage, this, std::placeholders::_1);
      ProtocolMessage* msg = new ProtocolMessage(protocol);
      msg->PutBuffer(data);
      if(sequenceNo)
      {
        AsyncEncrypt(msg, sendto);
      }
      else
      {
        AsyncGenIntro(msg, sendto);
      }
    }

    struct AsyncKeyExchange
    {
      llarp_logic* logic;
      llarp_crypto* crypto;
      byte_t* sharedKey;
      byte_t* remotePubkey;
      byte_t* localSeckey;
      byte_t* nonce;
      ProtocolMessage* msg = nullptr;
      std::function< void(ProtocolMessage*) > hook;

      AsyncKeyExchange(llarp_logic* l, llarp_crypto* c, byte_t* key,
                       byte_t* remote, byte_t* localSecret, byte_t* n)
          : logic(l)
          , crypto(c)
          , sharedKey(key)
          , remotePubkey(remote)
          , localSeckey(localSecret)
          , nonce(n)
      {
      }

      static void
      Work(void* user)
      {
        AsyncKeyExchange* self = static_cast< AsyncKeyExchange* >(user);
        self->crypto->dh_server(self->sharedKey, self->remotePubkey,
                                self->localSeckey, self->nonce);
      }
    };

    void
    Endpoint::OutboundContext::AsyncGenIntro(
        ProtocolMessage* msg, std::function< void(ProtocolMessage*) > result)
    {
      msg->N.Randomize();
      AsyncKeyExchange* ex = new AsyncKeyExchange(
          m_Parent->Logic(), m_Parent->Crypto(), sharedKey,
          currentIntroSet.A.enckey, m_Parent->GetEncryptionSecretKey(), msg->N);
      llarp_threadpool_queue_job(m_Parent->Worker(),
                                 {ex, &AsyncKeyExchange::Work});
    }

    void
    Endpoint::OutboundContext::SendMessage(ProtocolMessage* msg)
    {
      // TODO: delete msg
      // TODO: implement me
    }

    bool
    Endpoint::OutboundContext::SelectHop(llarp_nodedb* db, llarp_rc* prev,
                                         llarp_rc* cur, size_t hop)
    {
      // TODO: don't hard code
      if(hop == 3)
      {
        llarp_time_t lowest = 0xFFFFFFFFFFFFFFFFUL;
        Introduction chosen;
        // pick intro set with lowest latency
        for(const auto& intro : currentIntroSet.I)
        {
          if(intro.latency < lowest)
          {
            chosen = intro;
            lowest = intro.latency;
          }
        }
        auto localcopy = llarp_nodedb_get_rc(db, chosen.router);
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
              chosen);
          return false;
        }
      }
      else
        return llarp_pathbuilder_context::SelectHop(db, prev, cur, hop);
    }

    void
    Endpoint::OutboundContext::AsyncEncrypt(
        ProtocolMessage* msg, std::function< void(ProtocolMessage*) > result)
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

    byte_t*
    Endpoint::GetEncryptionSecretKey()
    {
      return m_Identity.enckey;
    }

  }  // namespace service
}  // namespace llarp
