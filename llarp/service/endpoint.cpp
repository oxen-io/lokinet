#include <llarp/dht/messages/findintro.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/service/endpoint.hpp>
#include "router.hpp"

namespace llarp
{
  namespace service
  {
    Endpoint::Endpoint(const std::string& name, llarp_router* r)
        : llarp_pathbuilder_context(r, r->dht, 2), m_Router(r), m_Name(name)
    {
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
      }
      if(k == "prefetch-tag")
      {
        m_PrefetchTags.insert(v);
      }
      return true;
    }

    void
    Endpoint::Tick()
    {
      if(ShouldPublishDescriptors())
      {
        std::list< Introduction > I;
        if(!GetCurrentIntroductions(I))
        {
          llarp::LogWarn("could not publish descriptors for endpoint ", Name(),
                         " because we couldn't get any introductions");
          return;
        }
        m_IntroSet.I = I;
        if(!m_Tag.IsZero())
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
      auto now = llarp_time_now_ms();
      for(const auto& tag : m_PrefetchTags)
      {
        auto itr = m_PrefetchedTags.find(tag);
        if(itr == m_PrefetchedTags.end())
        {
          // put cached result will try next iteration
          m_PrefetchedTags.emplace(tag, tag);
        }
        else if(itr->second.ShouldRefresh(now))
        {
          auto path = PickRandomEstablishedPath();
          if(path)
          {
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
          llarp::LogWarn(
              "invalid signature in got intro message for service endpoint ",
              Name());
          IntroSetPublishFail();
          return false;
        }
        if(m_Identity.pub == introset.A)
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
        const std::set< IntroSet >& results)
    {
      return true;
    }

    llarp::routing::IMessage*
    Endpoint::CachedTagResult::BuildRequestMessage()
    {
      llarp::routing::DHTMessage* msg = new llarp::routing::DHTMessage();
      msg->M.push_back(new llarp::dht::FindIntroMessage(tag, pendingTX));
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
            m_IntroSet, m_CurrentPublishTX));
        if(path->SendRoutingMessage(&msg, r))
        {
          m_LastPublishAttempt = llarp_time_now_ms();
          llarp::LogInfo(Name(), " publishing introset");
          return true;
        }
        return false;
      }
      else
        return false;
    }

    void
    Endpoint::IntroSetPublishFail()
    {
      llarp::LogWarn("failed to publish introset for ", Name());
      m_CurrentPublishTX = 0;
    }

    bool
    Endpoint::ShouldPublishDescriptors() const
    {
      auto now = llarp_time_now_ms();
      if(m_IntroSet.HasExpiredIntros())
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

    Endpoint::OutboundContext::OutboundContext(Endpoint* parent)
        : llarp_pathbuilder_context(parent->m_Router, parent->m_Router->dht, 2)
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
      // TODO: implement me
      return false;
    }
  }  // namespace service
}  // namespace llarp
