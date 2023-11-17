#pragma once

#include "common.hpp"

namespace llarp::RCFetchMessage
{
  inline constexpr auto INVALID_REQUEST = "Invalid relay ID requested."sv;

  inline static std::string
  serialize(std::chrono::system_clock::time_point since, const std::vector<RouterID>& explicit_ids)
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("since", since.time_since_epoch() / 1s);
      auto id_list = btdp.append_list("explicit_ids");
      for (const auto& rid : explicit_ids)
        id_list.append(rid.ToView());
    }
    catch (...)
    {
      log::error(link_cat, "Error: RCFetchMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }
}  // namespace llarp::RCFetchMessage
