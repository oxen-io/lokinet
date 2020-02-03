#ifndef LLARP_SERVICE_TAG_LOOKUP_JOB_HPP
#define LLARP_SERVICE_TAG_LOOKUP_JOB_HPP

#include <routing/message.hpp>
#include <service/intro_set.hpp>
#include <service/lookup.hpp>
#include <service/tag.hpp>
#include <util/types.hpp>

#include <set>

namespace llarp
{
  namespace service
  {
    struct Endpoint;

    struct CachedTagResult
    {
      const static llarp_time_t TTL = 10000;
      llarp_time_t lastRequest      = 0;
      llarp_time_t lastModified     = 0;
      std::set< EncryptedIntroSet > result;
      Tag tag;
      Endpoint* m_parent;

      CachedTagResult(const Tag& t, Endpoint* p) : tag(t), m_parent(p)
      {
      }

      ~CachedTagResult() = default;

      void
      Expire(llarp_time_t now);

      bool
      ShouldRefresh(llarp_time_t now) const
      {
        if(now <= lastRequest)
          return false;
        return (now - lastRequest) > TTL;
      }

      std::shared_ptr< routing::IMessage >
      BuildRequestMessage(uint64_t txid);

      bool
      HandleResponse(const std::set< EncryptedIntroSet >& results);
    };

    struct TagLookupJob : public IServiceLookup
    {
      TagLookupJob(Endpoint* parent, CachedTagResult* result);

      ~TagLookupJob() override = default;

      std::shared_ptr< routing::IMessage >
      BuildRequestMessage() override
      {
        return m_result->BuildRequestMessage(txid);
      }

      bool
      HandleResponse(const std::set< EncryptedIntroSet >& results) override
      {
        return m_result->HandleResponse(results);
      }

      CachedTagResult* m_result;
    };

  }  // namespace service
}  // namespace llarp

#endif
