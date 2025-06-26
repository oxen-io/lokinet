#pragma once

#include <llarp/address/address.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/logging/buffer.hpp>

namespace llarp
{
    namespace ONION
    {
        std::string serialize_frames(std::vector<std::string>&& frames);

        std::vector<std::string> deserialize_frames(std::string_view&& buf);

        std::string serialize_hop(std::string_view key, const SymmNonce& nonce, const std::string& encrypted);

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
            std::string serialize(std::string endpoint, std::string payload);

            std::string serialize_aligned(std::string payload, const HopID& pivot_txid);

            std::pair<std::string, std::string> deserialize(oxenc::bt_dict_consumer&& btdc);

        }  // namespace CONTROL

        namespace DATA
        {
            std::string serialize(std::string payload, const RouterID& local);

            std::string serialize_intermediate(std::string payload, const HopID& pivot_txid);

            std::string serialize_inner(std::string body, session_tag tag);

            std::pair<NetworkAddress, bstring> deserialize(oxenc::bt_dict_consumer&& btdc);

            std::pair<HopID, std::string> deserialize_intermediate(oxenc::bt_dict_consumer&& btdc);

            std::pair<session_tag, std::vector<uint8_t>> deserialize_inner(std::string&& payload);

        }  // namespace DATA

    }  // namespace PATH

}  // namespace llarp
