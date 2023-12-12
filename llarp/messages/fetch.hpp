#pragma once

#include "common.hpp"

#include <llarp/util/logging/buffer.hpp>

namespace llarp
{
  namespace FetchRCMessage
  {
    inline const auto INVALID_REQUEST =
        messages::serialize_response({{messages::STATUS_KEY, "Invalid relay ID requested"}});

    inline static std::string
    serialize(
        std::chrono::system_clock::time_point since, const std::vector<RouterID>& explicit_ids)
    {
      oxenc::bt_dict_producer btdp;

      try
      {
        {
          auto sublist = btdp.append_list("explicit_ids");

          for (const auto& rid : explicit_ids)
            sublist.append(rid.ToView());
        }

        btdp.append("since", since.time_since_epoch() / 1s);
      }
      catch (...)
      {
        log::error(link_cat, "Error: RCFetchMessage failed to bt encode contents!");
      }

      return std::move(btdp).str();
    }
  }  // namespace FetchRCMessage

  namespace BootstrapFetchMessage
  {
    // the LocalRC is converted to a RemoteRC type to send to the bootstrap seed
    inline static std::string
    serialize(const LocalRC& local_rc, size_t quantity)
    {
      oxenc::bt_dict_producer btdp;
      btdp.append_encoded("local", oxen::quic::to_sv(local_rc.view()));
      log::critical(logcat, "Serializing localRC: {}", oxenc::to_hex(local_rc.view()));
      btdp.append("quantity", quantity);
      return std::move(btdp).str();
    }

    inline static std::string
    serialize_response(const std::vector<RouterID>& explicit_ids)
    {
      oxenc::bt_dict_producer btdp;

      try
      {
        auto sublist = btdp.append_list("explicit_ids");

        for (const auto& rid : explicit_ids)
          sublist.append(rid.ToView());
      }
      catch (...)
      {
        log::error(link_cat, "Error: BootstrapFetchMessage failed to bt encode contents!");
      }

      return std::move(btdp).str();
    }
  }  // namespace BootstrapFetchMessage

  namespace FetchRIDMessage
  {
    inline constexpr auto INVALID_REQUEST = "Invalid relay ID requested to relay response from."sv;

    inline static std::string
    serialize(const RouterID& source)
    {
      // serialize_response is a bit weird here, and perhaps could have a sister function
      // with the same purpose but as a request, but...it works.
      return messages::serialize_response({{"source", source.ToView()}});
    }
  }  // namespace FetchRIDMessage

}  // namespace llarp