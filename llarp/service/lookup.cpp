#include <service/lookup.hpp>

#include <path/path.hpp>
#include <util/time.hpp>
#include <router/abstractrouter.hpp>
#include <util/thread/logic.hpp>
#include <utility>

namespace llarp
{
  struct AbstractRouter;

  namespace service
  {
    IServiceLookup::IServiceLookup(ILookupHolder *p, uint64_t tx, std::string n,
                                   size_t requestsPerSend)
        : m_parent(p)
        , txid(tx)
        , name(std::move(n))
        , numRequestsPerSend(requestsPerSend)
    {
      m_created = time_now_ms();
      p->PutLookup(this, tx);
    }

    bool
    IServiceLookup::SendRequestViaPath(path::Path_ptr path, AbstractRouter *r)
    {
      auto msg = BuildRequestMessage();
      if(!msg)
        return false;
      endpoint = path->Endpoint();
      LogicCall(r->logic(), [=]() { path->SendRoutingMessage(*msg, r); });

      // TODO: sub class should specify number of requests per call here
      responsesExpected += numRequestsPerSend;
      return true;
    }

    bool
    IServiceLookup::HandleResponse(
        const std::set< EncryptedIntroSet > &introset)
    {
      bool handled = OnHandleResponse(introset);
      if(handled)
      {
        responsesReceived++;
        LogInfo("Lookup ", txid, " has received ",
                responsesReceived, " of ",
                responsesExpected, " responses");
      }

      if(responsesReceived == responsesExpected)
      {
        OnAllResponsesReceived();
        return true;
      }
      else
      {
        return false;
      }
    }

    bool
    IServiceLookup::IsTimedOut(llarp_time_t now, llarp_time_t timeout) const
    {
      if(now <= m_created)
        return false;
      return (responsesReceived == 0) and (now - m_created > timeout);
    }

  }  // namespace service
}  // namespace llarp
