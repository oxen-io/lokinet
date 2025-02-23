#pragma once

#include "common.hpp"

#include <llarp/util/logging/buffer.hpp>

namespace llarp
{
    namespace GossipRC
    {
        inline static std::string serialize(const RouterID& last_sender, const RemoteRC& rc)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append_encoded("r", rc.view());
            btdp.append("s", last_sender.to_view());

            return std::move(btdp).str();
        }
    }  // namespace GossipRC

    namespace BootstrapFetch
    {
        // the LocalRC is converted to a RemoteRC type to send to the bootstrap seed
        inline static std::string serialize(std::optional<LocalRC> local_rc, size_t quantity)
        {
            oxenc::bt_dict_producer btdp;

            if (local_rc)
            {
                log::trace(messages::logcat, "Serializing localRC: {}", oxenc::to_hex(local_rc->view()));
                btdp.append_encoded("l", local_rc->view());
            }

            btdp.append("q", quantity);

            return std::move(btdp).str();
        }
    }  // namespace BootstrapFetch

    namespace FetchRC
    {
        inline const auto INVALID_REQUEST =
            messages::serialize_response({{messages::STATUS_KEY, "Invalid relay ID requested"}});

        inline static std::string serialize(const RouterID& rid)
        {
            oxenc::bt_dict_producer btdp;

            auto sublist = btdp.append_list("x");
            sublist.append(rid.to_view());

            return std::move(btdp).str();
        }

        inline static std::string serialize(const std::vector<RouterID>& explicit_ids)
        {
            oxenc::bt_dict_producer btdp;

            auto sublist = btdp.append_list("x");

            for (const auto& rid : explicit_ids)
                sublist.append(rid.to_view());

            return std::move(btdp).str();
        }

        inline static std::set<RemoteRC> deserialize_response(oxenc::bt_dict_consumer&& btdc)
        {
            try
            {
                std::set<RemoteRC> rcs{};

                btdc.required("r");
                {
                    auto sublist = btdc.consume_list_consumer();

                    while (not sublist.is_finished())
                        rcs.emplace(sublist.consume_dict_data());
                }

                return rcs;
            }
            catch (...)
            {
                throw;
            }
        }
    }  // namespace FetchRC

    namespace FetchRID
    {
        inline constexpr auto INVALID_REQUEST = "Invalid relay ID requested to relay response from."sv;

        inline static std::string serialize(const RouterID& source)
        {
            oxenc::bt_dict_producer btdp;
            btdp.append("s", source.to_view());
            return std::move(btdp).str();
        }
    }  // namespace FetchRID

}  // namespace llarp
