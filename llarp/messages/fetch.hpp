#pragma once

#include "common.hpp"

#include <llarp/util/logging/buffer.hpp>

namespace llarp
{
  namespace GossipRCMessage
  {
    inline static std::string
    serialize(const RouterID& last_sender, const RemoteRC& rc)
    {
      oxenc::bt_dict_producer btdp;

      try
      {
        btdp.append_encoded("rc", rc.view());
        btdp.append("sender", last_sender.ToView());
      }
      catch (...)
      {
        log::error(link_cat, "Error: GossipRCMessage failed to bt encode contents");
      }

      return std::move(btdp).str();
    }
  }  // namespace GossipRCMessage

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
    serialize(std::optional<LocalRC> local_rc, size_t quantity)
    {
      oxenc::bt_dict_producer btdp;

      if (local_rc)
      {
        log::critical(logcat, "Serializing localRC: {}", oxenc::to_hex(local_rc->view()));
        btdp.append_encoded("local", oxen::quic::to_sv(local_rc->view()));
      }

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
      oxenc::bt_dict_producer btdp;

      try
      {
        btdp.append("source", source.ToView());
      }
      catch (...)
      {
        log::error(link_cat, "Error: FetchRIDMessage failed to bt encode contents!");
      }

      return std::move(btdp).str();
    }
  }  // namespace FetchRIDMessage

}  // namespace llarp
