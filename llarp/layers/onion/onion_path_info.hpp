#pragma once

#include <functional>

namespace llarp::layers::onion
{
  /// all informational data about a singular onion path we created.
  struct OnionPathInfo
  {
    bool
    ready_to_use() const;
  };
}  // namespace llarp::layers::onion

namespace std
{
  template <>
  struct hash<llarp::layers::onion::OnionPathInfo>
  {
    size_t
    operator()(const llarp::layers::onion::OnionPathInfo&) const
    {
      // TODO: implement me
      return size_t{};
    }
  };
}  // namespace std
