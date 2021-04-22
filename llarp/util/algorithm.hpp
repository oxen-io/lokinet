#pragma once

#include <algorithm>

namespace llarp::util
{
  /// remove items from a container if a predicate is true
  /// return the number of items removed
  constexpr auto erase_if = [](auto& container, auto&& pred) -> std::size_t {
    std::size_t removed = 0;
    for (auto itr = container.begin(); itr != container.end();)
    {
      if (pred(*itr))
      {
        itr = container.erase(itr);
        removed++;
      }
      else
        itr++;
    }
    return removed;
  };
}  // namespace llarp::util
