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
        responsesExpected--;
        LogInfo("Lookup ", txid, " has received ",
            (numRequestsPerSend - responsesExpected), " of ",
            numRequestsPerSend, " responses");
      }

      if (responsesExpected == 0)
      {
        OnAllResponsesReceived();
      }

      return (responsesExpected == 0);
    }

  }  // namespace service
}  // namespace llarp
