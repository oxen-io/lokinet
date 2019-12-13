#include <service/name_lookup_batch_job.hpp>
#include <dht/messages/findintro.hpp>
#include <routing/dht_message.hpp>

namespace llarp
{
  namespace service
  {
    struct NameLookupJob : public IServiceLookup
    {
      bool
      HandleResponse(const std::set< IntroSet >& results)
      {
        if(results.empty())
          return false;
        m_Parent->results[endpoint] = results.begin()->A.Addr();
      }

      NameLoookupBatchJob* const m_Parent;
    };

    bool
    NameLookupBatchJob::MakeRequest(Endpoint* e, const path::Path_ptr& path,
                                    uint64_t txid)
    {
      auto job = new NameLookupJob(e, Name, txid, this);
      return job->SendRequestViaPath(path, e->Router()))
    }
  }  // namespace service
}  // namespace llarp
