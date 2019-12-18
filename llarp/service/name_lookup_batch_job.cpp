#include <service/name_lookup_batch_job.hpp>
#include <dht/messages/findintro.hpp>
#include <routing/dht_message.hpp>
#include <service/endpoint.hpp>

namespace llarp
{
  namespace service
  {
    struct NameLookupJob final : public IServiceLookup
    {
      NameLookupJob(Endpoint* e, uint64_t txid,
                    std::shared_ptr< NameLookupBatchJob > b, const RouterID r)
          : IServiceLookup(e, txid, "NameLookupJob")
          , m_ParentJob(std::move(b))
          , m_Endpoint(r)
      {
      }

      std::shared_ptr< routing::IMessage >
      BuildRequestMessage() override
      {
        auto msg = std::make_shared< routing::DHTMessage >();
        msg->M.emplace_back(
            std::make_unique< dht::FindIntroMessage >(txid, m_ParentJob->Name));
        return msg;
      }

      bool
      HandleResponse(const std::set< IntroSet >& results)
      {
        if(results.empty())
        {
          m_ParentJob->AddResponse(m_Endpoint, {});
        }
        else
        {
          for(const auto& introset : results)
            m_ParentJob->AddResponse(m_Endpoint, introset);
        }
        if(m_ParentJob->HasEnoughVotes())
        {
          return m_ParentJob->HandleResult();
        }
        return true;
      }

      std::shared_ptr< NameLookupBatchJob > m_ParentJob;
      const RouterID m_Endpoint;
    };

    bool
    NameLookupBatchJob::MakeRequest(const path::Path_ptr& path, uint64_t txid)
    {
      if(endpoint == nullptr)
        return false;
      auto job = new NameLookupJob(endpoint, txid, shared_from_this(),
                                   path->Endpoint());
      if(not job->SendRequestViaPath(path, endpoint->Router()))
        return false;
      m_NumRequestsMade++;
      return true;
    }

    bool
    NameLookupBatchJob::HasEnoughVotes() const
    {
      return results.size()
          == (m_NumRequestsMade - consensusAlgo.DisagreementThreshold);
    }

    bool
    NameLookupBatchJob::HandleResult()
    {
      std::unordered_map< service::Address, uint16_t, service::Address::Hash >
          hits;
      for(const auto& item : results)
      {
        if(item.second.has_value())
        {
          const service::Address addr = item.second.value();
          auto itr                    = hits.find(addr);
          if(itr == hits.end())
          {
            hits[addr] = 1;
            continue;
          }
          hits[addr] += 1;
        }
      }

      auto agreed = consensusAlgo.ExtractAgreement(hits, results.size());
      if(agreed.has_value())
      {
        // concensous agreement
        auto itr = introsets.find(agreed.value());
        if(itr != introsets.end())
        {
          // put introset as outbound context
          endpoint->PutNewOutboundContext(itr->second);
          handler(itr->first);
        }
        else
        {
          LogError("No introset found for ", Name, " after name agreement?");
          handler({});
        }
        handler = nullptr;
        return true;
      }
      else if(results.size() < m_NumRequestsMade)
      {
        // still can get more results after no agreement
        return true;
      }
      // no agreement and reached end of lifecycle
      handler({});
      handler = nullptr;
      return false;
    }

    void
    NameLookupBatchJob::AddResponse(const RouterID ep,
                                    absl::optional< IntroSet > result)
    {
      // ensure no collisions
      {
        auto itr = results.find(ep);
        if(itr != results.end())
        {
          if(itr->second.has_value())
          {
            LogError("name lookup for ", Name, " got multiple results from ",
                     ep,
                     " that conflicts with previous result from themself: '",
                     itr->second.value(), "'");
          }
          return;
        }
      }
      if(result.has_value())
      {
        // got response
        const Address addr = result->A.Addr();
        auto itr           = introsets.find(addr);
        if(itr != introsets.end())
        {
          // update introset if newer
          if(itr->second < result.value())
          {
            itr->second = result.value();
          }
        }
        results[ep] = addr;
      }
      else
      {
        // no response
        results[ep] = {};
      }
    }

  }  // namespace service
}  // namespace llarp
