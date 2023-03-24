#include "onion_stats.hpp"
#include <unordered_set>

namespace llarp::layers::onion
{
  bool
  OnionStats::ready() const
  {
    // todo: implement me
    return false;
  }

  std::unordered_set<OnionPathInfo>
  OnionStats::built_paths() const
  {
    // todo: implement me
    return {};
  }
}  // namespace llarp::layers::onion
