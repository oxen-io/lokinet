#pragma once

#include <llarp/layers/flow/flow_addr.hpp>
#include <llarp/util/decaying_hashtable.hpp>

namespace llarp::layers::platform
{
  /// cache for in network names (ONS records) with static TTL.
  /// has no negative lookup cache.
  class NameCache
  {
    /// a cache of all the names we have looked up.
    /// note: unlike other uses in the code, the decaying hashtabe this wraps uses monotonic uptime
    /// instead of unix timestamps.
    util::DecayingHashTable<std::string, flow::FlowAddr> _names;

   public:
    NameCache(std::chrono::seconds ttl = 5min);

    /// apply any cache expiry then get an entry from the cache if it exists.
    std::optional<flow::FlowAddr>
    get(std::string name);

    /// apply any cache expiry and then put a name into the cache.
    void
    put(std::string name, flow::FlowAddr addr);
  };
}  // namespace llarp::layers::platform
