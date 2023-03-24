#include "name_cache.hpp"

namespace llarp::layers::flow
{
  NameCache::NameCache(std::chrono::seconds ttl) : _names{ttl}
  {}

  std::optional<FlowAddr>
  NameCache::get(std::string name)
  {
    auto now = uptime();
    _names.Decay(now);
    return _names.Get(std::move(name));
  }

  void
  NameCache::put(std::string name, FlowAddr addr)
  {
    auto now = uptime();
    _names.Decay(now);
    _names.Put(name, addr, now);
  }
}  // namespace llarp::layers::flow
