#include "name_cache.hpp"
#include "llarp/layers/flow/flow_addr.hpp"
#include "llarp/util/time.hpp"

namespace llarp::layers::platform
{
  NameCache::NameCache(std::chrono::seconds ttl) : _names{ttl}
  {}

  std::optional<flow::FlowAddr>
  NameCache::get(std::string name)
  {
    auto now = uptime();
    _names.Decay(now);
    return _names.Get(std::move(name));
  }

  void
  NameCache::put(std::string name, flow::FlowAddr addr)
  {
    auto now = uptime();
    _names.Decay(now);
    _names.Put(name, addr, now);
  }
}  // namespace llarp::layers::platform
