#include "fetch.hpp"

#include "common.hpp"

namespace llarp
{
    namespace GossipRC
    {
        std::vector<std::byte> serialize(const RouterID& last_sender, const RemoteRC& rc)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append_encoded("r", rc.view());
            btdp.append("s", last_sender.span());

            return to_bytes(btdp);
        }
    }  // namespace GossipRC

    namespace BootstrapFetch
    {
        // the LocalRC is converted to a RemoteRC type to send to the bootstrap seed
        std::vector<std::byte> serialize(std::optional<LocalRC> local_rc, size_t quantity)
        {
            oxenc::bt_dict_producer btdp;

            if (local_rc)
                btdp.append_encoded("l", local_rc->view());

            btdp.append("q", quantity);

            return to_bytes(btdp);
        }
    }  // namespace BootstrapFetch

    namespace FetchRC
    {
        const std::string INVALID_REQUEST = messages::serialize_status_response("Invalid relay ID requested");

        std::vector<std::byte> serialize(const std::vector<RouterID>& explicit_ids)
        {
            oxenc::bt_dict_producer btdp;

            {
                auto sublist = btdp.append_list("x");
                for (const auto& rid : explicit_ids)
                    sublist.append(rid.span());
            }

            return to_bytes(btdp);
        }

        std::vector<RemoteRC> deserialize_response(NetID netid, oxenc::bt_dict_consumer&& btdc)
        {
            std::vector<RemoteRC> rcs;

            for (auto sublist = btdc.require<oxenc::bt_list_consumer>("r"); not sublist.is_finished();)
                rcs.emplace_back(sublist.consume_dict_data(), netid);

            return rcs;
        }
    }  // namespace FetchRC

    namespace FetchRID
    {
        std::vector<std::byte> serialize(const RouterID& source)
        {
            oxenc::bt_dict_producer btdp;
            btdp.append("s", source.span());
            return to_bytes(btdp);
        }
    }  // namespace FetchRID

}  // namespace llarp
