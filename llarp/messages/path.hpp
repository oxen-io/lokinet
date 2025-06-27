#pragma once

#include <llarp/address/address.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/logging/buffer.hpp>

namespace llarp
{
    namespace ONION
    {
        std::string serialize_frames(const std::vector<std::string>& frames);

        std::vector<std::string> deserialize_frames(std::string_view buf);

        std::string serialize_hop(
            std::span<const std::byte> key, const SymmNonce& nonce, std::span<const std::byte> encrypted);

        std::pair<std::string, shared_kx_data> deserialize_decrypt(
            oxenc::bt_dict_consumer&& btdc, const Ed25519SecretKey& local_sk);

        std::tuple<RouterID, SymmNonce, std::string> deserialize(oxenc::bt_dict_consumer&& btdc);

        std::tuple<HopID, SymmNonce, std::string> deserialize_hop(oxenc::bt_dict_consumer&& btdc);

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
            std::string serialize(std::string_view endpoint, std::span<const std::byte> payload);

            std::string serialize_aligned(std::span<const std::byte> payload, const HopID& pivot_txid);

            std::pair<std::string, std::string> deserialize(oxenc::bt_dict_consumer&& btdc);

        }  // namespace CONTROL

        namespace DATA
        {
            std::string serialize(std::string_view payload, const RouterID& local);

            std::string serialize_intermediate(
                const session_tag& tag, std::span<const std::byte> payload, const HopID& pivot_txid);

            std::pair<NetworkAddress, std::span<const std::byte>> deserialize(oxenc::bt_dict_consumer&& btdc);

            std::pair<HopID, std::string> deserialize_intermediate(oxenc::bt_dict_consumer&& btdc);

            std::pair<session_tag, std::span<std::byte>> deserialize_inner(std::span<std::byte> payload);

        }  // namespace DATA

    }  // namespace PATH

}  // namespace llarp
