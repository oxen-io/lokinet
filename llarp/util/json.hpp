#pragma once

#include <nlohmann/json.hpp>
#include <optional>

namespace llarp::json
{
  /// maybe get a sub element in a dict by key or return a fallback
  template <typename T>
  std::optional<T>
  maybe_get(
      const nlohmann::json& obj, std::string_view key, std::optional<T> fallback = std::nullopt)
  {
    if (auto itr = obj.find(key); itr != obj.end())
      return itr->get<T>();
    return fallback;
  }
}  // namespace llarp::json
