#pragma once

#include <unordered_set>
#include "onion_path_info.hpp"

namespace llarp::layers::onion
{
  struct OnionStats
  {
    std::unordered_set<OnionPathInfo> path;

    double path_success_ratio;

    /// set to true if we are routing transit traffic as a service node.
    bool allowing_transit;

    /// return true if we have enough paths built to operate as a client.
    bool
    ready() const;

    /// return a set of all paths that are built.
    std::unordered_set<OnionPathInfo>
    built_paths() const;
  };

}  // namespace llarp::layers::onion
