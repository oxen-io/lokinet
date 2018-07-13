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
      return true;
    }

    void
    Endpoint::Tick()
    {
      if(ShouldPublishDescriptors())
      {
        llarp::LogDebug("publish descriptor for endpoint ", m_Name);
        IntroSet introset;
        if(!GetCurrentIntroductions(introset.I))
        {
          llarp::LogWarn("could not publish descriptors for endpoint ", m_Name,
                         " because we couldn't get any introductions");
          return;
        }
        if(!m_Identity.SignIntroSet(introset, &m_Router->crypto))
        {
          llarp::LogWarn("failed to sign introset for endpoint ", m_Name);
          return;
        }
        if(PublishIntroSet(introset, m_Router))
        {
          llarp::LogInfo("publishing introset for endpoint ", m_Name);
        }
        else
        {
          llarp::LogWarn("failed to publish intro set for endpoint ", m_Name);
        }
      }
    }

    bool
    Endpoint::HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg)
    {
      auto crypto = &m_Router->crypto;
      for(const auto& introset : msg->I)
      {
        if(!introset.VerifySignature(crypto))
        {
          llarp::LogWarn(
              "invalid signature in got intro message for service endpoint ",
              m_Name);
          return false;
        }
        if(m_Identity.pub == introset.A)
        {
          llarp::LogInfo(
              "got introset publish confirmation for hidden service endpoint ",
              m_Name);
        }
        else
        {
          /// TODO: implement lookup response
        }
      }
      return true;
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