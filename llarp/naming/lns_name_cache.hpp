#ifndef LLARP_NAMING_LNS_NAME_CACHE_HPP
#define LLARP_NAMING_LNS_NAME_CACHE_HPP
#include <util/time.hpp>
#include <unordered_map>
#include <naming/i_name_lookup_handler.hpp>

namespace llarp
{
  namespace naming
  {
    using Addr_t           = llarp::service::Address;
    using NameCacheEntry_t = std::pair< Addr_t, llarp_time_t >;
    using Name_t           = std::string;
    using NameCache_Super  = std::unordered_map< Name_t, NameCacheEntry_t >;

    struct NameCache : public NameCache_Super
    {
      using Super_t = NameCache_Super;
      /// now long to cache for
      static constexpr llarp_time_t TTL = 60 * 60 * 1000;

      NameCache() = default;

      /// decay old elements in cache
      void
      Decay(llarp_time_t now = 0);

      /// get a valid cache item by name or look it up if cache miss or expired
      /// cache entry
      void
      GetCachedOrLookupAsync(const Name_t name, INameLookupHandler& lookup,
                             NameLookupResultHandler h);
    };
  }  // namespace naming
}  // namespace llarp

#endif
