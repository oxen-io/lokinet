#pragma once

#include <llarp/contact/relay_contact.hpp>
#include <llarp/contact/router_id.hpp>
#include <llarp/util/logging/buffer.hpp>

namespace llarp
{
    namespace GossipRC
    {
        std::vector<std::byte> serialize(const RouterID& last_sender, const RemoteRC& rc);

    }  // namespace GossipRC

    namespace BootstrapFetch
    {
        // the LocalRC is converted to a RemoteRC type to send to the bootstrap seed
        std::vector<std::byte> serialize(std::optional<LocalRC> local_rc, size_t quantity);

    }  // namespace BootstrapFetch

    namespace FetchRC
    {
        extern const std::string INVALID_REQUEST;

        std::vector<std::byte> serialize(std::span<const RouterID> explicit_ids);

        std::vector<RemoteRC> deserialize_response(NetID netid, oxenc::bt_dict_consumer&& btdc);

    }  // namespace FetchRC

    namespace FetchRID
    {
        inline constexpr auto INVALID_REQUEST = "Invalid relay ID requested to relay response from."sv;

        std::vector<std::byte> serialize(const RouterID& source);

    }  // namespace FetchRID

}  // namespace llarp
