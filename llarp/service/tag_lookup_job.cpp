#include <service/tag_lookup_job.hpp>

#include <dht/messages/findintro.hpp>
#include <routing/dht_message.hpp>
#include <service/endpoint.hpp>

namespace llarp
{
  namespace service
  {
    bool
    CachedTagResult::HandleResponse(const std::set< EncryptedIntroSet >&)
    {
      return true;
    }

    void
    CachedTagResult::Expire(llarp_time_t now)
    {
      auto itr = result.begin();
      while(itr != result.end())
      {
        if(itr->IsExpired(now))
        {
          itr          = result.erase(itr);
          lastModified = now;
        }
        else
        {
          ++itr;
        }
      }
    }

    std::shared_ptr< routing::IMessage >
    CachedTagResult::BuildRequestMessage(uint64_t txid)
    {
      auto msg = std::make_shared< routing::DHTMessage >();
      msg->M.emplace_back(std::make_unique< dht::FindIntroMessage >(tag, txid));
      lastRequest = m_parent->Now();
      return msg;
    }

    TagLookupJob::TagLookupJob(Endpoint* parent, CachedTagResult* result)
        : IServiceLookup(parent, parent->GenTXID(), "taglookup")
        , m_result(result)
    {
    }
  }  // namespace service

}  // namespace llarp
