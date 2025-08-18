#pragma once

#include "llarp/constants/path.hpp"

#include <llarp/address/address.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/logging/buffer.hpp>

namespace llarp
{
    namespace ONION
    {
        /*
        std::string serialize_frames(const std::array<std::string, path::BUILD_LENGTH>& frames);

        std::array<std::string, path::BUILD_LENGTH> deserialize_frames(std::string_view buf);
        */

        // Serializes a *non-data* message
        std::vector<std::byte> serialize_stream_hop(
            const HopID& hopid, const SymmNonce& nonce, std::span<const std::byte> encrypted);

        std::tuple<std::string, SharedSecret, SymmNonce> deserialize_decrypt(
            oxenc::bt_dict_consumer&& btdc, const Ed25519SecretKey& local_sk);

        std::tuple<HopID, SymmNonce, std::vector<std::byte>> deserialize_stream_hop(oxenc::bt_dict_consumer&& btdc);

    }  // namespace ONION

    namespace PATH
    {
        namespace BUILD
        {
            extern const std::string NO_TRANSIT;
            extern const std::string BAD_LIFETIME;
            extern const std::string BAD_FRAMES;
            extern const std::string BAD_PATHID;
            extern const std::string BAD_CRYPTO;

            std::string serialize_hop(path::TransitHop& hop);

            std::shared_ptr<path::TransitHop> deserialize_hop(
                oxenc::bt_dict_consumer&& btdc, Router& r, const RouterID& src);

        }  // namespace BUILD

        namespace CONTROL
        {
            std::vector<std::byte> serialize(std::string_view endpoint, std::span<const std::byte> payload);

            std::vector<std::byte> serialize_aligned(std::span<const std::byte> payload, const HopID& pivot_txid);

            std::pair<std::string, std::string> deserialize(oxenc::bt_dict_consumer&& btdc);

        }  // namespace CONTROL

        namespace DATA
        {
            std::string serialize(std::string_view payload, const RouterID& local);

            std::pair<NetworkAddress, std::span<const std::byte>> deserialize(oxenc::bt_dict_consumer&& btdc);

            std::pair<HopID, std::string> deserialize_intermediate(oxenc::bt_dict_consumer&& btdc);

            std::pair<session_tag, std::span<std::byte>> deserialize_inner(std::span<std::byte> payload);

        }  // namespace DATA

    }  // namespace PATH

}  // namespace llarp
