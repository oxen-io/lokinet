#include <naming/lns_name_cache.hpp>

namespace llarp
{
  namespace naming
  {
    void
    NameCache::Decay(llarp_time_t now)
    {
      if(now == 0)
        now = llarp::time_now_ms();

      auto itr = Super_t::begin();
      while(itr != Super_t::end())
      {
        if(itr->second.second >= now)
          itr = Super_t::erase(itr);
        else
          ++itr;
      }
    }

    void
    NameCache::GetCachedOrLookupAsync(const Name_t name,
                                      INameLookupHandler& lookup,
                                      NameLookupResultHandler h)
    {
      auto itr = Super_t::find(name);
      if(itr != Super_t::end())
      {
        const auto now = llarp::time_now_ms();
        if(itr->second.second < now)
        {
          // valid entry not expired
          h(itr->second.first);
          return;
        }
      }
      lookup.LookupNameAsync(
          name,
          [h, name,
           self = this](absl::optional< llarp::service::Address > result) {
            // put cache entry on success
            if(result.has_value())
            {
              auto& entry  = (*self)[name];
              entry.first  = result.value();
              entry.second = llarp::time_now_ms() + TTL;
            }
            h(result);
          });
    }

  }  // namespace naming
}  // namespace llarp
