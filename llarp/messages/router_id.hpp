#pragma once

#include "common.hpp"

namespace llarp::RouterIDFetch
{
  inline constexpr auto INVALID_REQUEST = "Invalid relay ID requested to relay response from."sv;

  inline static std::string
  serialize(const RouterID& source)
  {
    // serialize_response is a bit weird here, and perhaps could have a sister function
    // with the same purpose but as a request, but...it works.
    return messages::serialize_response({{"source", source.ToView()}});
  }

}  // namespace llarp::RouterIDFetch
