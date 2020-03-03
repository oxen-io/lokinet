#ifndef LLARP_UTIL_STATUS_HPP
#define LLARP_UTIL_STATUS_HPP

#include <util/string_view.hpp>

#include <nlohmann/json.hpp>

#include <vector>
#include <string>
#include <algorithm>

namespace llarp
{
  namespace util
  {
    using StatusObject = nlohmann::json;
  }  // namespace util
}  // namespace llarp

#endif
