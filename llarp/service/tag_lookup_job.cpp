#include <service/tag_lookup_job.hpp>

#include <dht/messages/findintro.hpp>
#include <messages/dht.hpp>
#include <service/endpoint.hpp>

namespace llarp
{
  namespace service
  {
    bool
    CachedTagResult::HandleResponse(const std::set< IntroSet >& introsets)
    {
      auto now = parent->Now();

      for(const auto& introset : introsets)
        if(result.insert(introset).second)
          lastModified = now;
      LogInfo("Tag result for ", tag.ToString(), " got ", introsets.size(),
              " results from lookup, have ", result.size(),
              " cached last modified at ", lastModified, " is ",
              now - lastModified, "ms old");
      return true;
    }

    void
    CachedTagResult::Expire(llarp_time_t now)
    {
      auto itr = result.begin();
      while(itr != result.end())
      {
        if(itr->HasExpiredIntros(now))
        {
          LogInfo("Removing expired tag Entry ", itr->A.Name());
          itr          = result.erase(itr);
          lastModified = now;
        }
        else
        {
          ++itr;
        }
      }
    }

    routing::IMessage*
    CachedTagResult::BuildRequestMessage(uint64_t txid)
    {
      routing::DHTMessage* msg = new routing::DHTMessage();
      msg->M.emplace_back(new dht::FindIntroMessage(tag, txid));
      lastRequest = parent->Now();
      return msg;
    }

    TagLookupJob::TagLookupJob(Endpoint* parent, CachedTagResult* result)
        : IServiceLookup(parent, parent->GenTXID(), "taglookup")
        , m_result(result)
    {
    }
  }  // namespace service

}  // namespace llarp
