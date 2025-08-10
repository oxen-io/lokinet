#include "path.hpp"

#include "common.hpp"
#include "llarp/constants/path.hpp"
#include "llarp/crypto/crypto.hpp"

#include <llarp/util/bspan.hpp>

#include <ranges>
#include <stdexcept>

namespace llarp
{

    static auto logcat = llarp::log::Cat("path.msgs");

    // FIXME TODO: get rid of this file.  Serialization belongs with the thing being serialized, not
    // in some header far removed.
    namespace ONION
    {
        /** Serializes a path non-data (i.e. stream message).  Bt-encoded contents:
            - 'h' : Next upstream HopID
            - 'n' : Symmetric nonce used to encrypt the layer
            - 'x' : Encrypted payload transmitted to next recipient
        */
        std::vector<std::byte> serialize_stream_hop(
            const HopID& hopid, const SymmNonce& nonce, std::span<const std::byte> encrypted)
        {
            oxenc::bt_dict_producer btdp;
            btdp.append("h", hopid.span());
            btdp.append("n", nonce.span());
            btdp.append("x", encrypted);
            auto res = btdp.span<std::byte>();
            return {res.begin(), res.end()};
        }

        std::tuple<std::string, SharedSecret, SymmNonce> deserialize_decrypt(
            oxenc::bt_dict_consumer&& btdc, const Ed25519SecretKey& local_sk)
        {
            std::tuple<std::string, SharedSecret, SymmNonce> ret;
            auto& [payload, shared_secret, xor_nonce] = ret;

            PubKey eph_pubkey;
            SymmNonce dh_nonce;
            try
            {
                eph_pubkey.assign(btdc.require_span<std::byte, PubKey::SIZE>("k"));
                dh_nonce.assign(btdc.require_span<std::byte, SymmNonce::SIZE>("n"));
                payload = btdc.require<std::string>("x");
                btdc.finish();
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception caught deserializing onion data: {}", e.what());
                throw std::runtime_error{messages::ERROR_RESPONSE};
            }

            log::trace(logcat, "payload: {}", buffer_printer{payload});

            try
            {
                if (!crypto::dh_server(shared_secret, eph_pubkey, local_sk, dh_nonce))
                    throw std::runtime_error{"Failed to derive hop path shared secret"};
                crypto::xchacha20(as_bspan(payload), shared_secret, dh_nonce);

                log::trace(logcat, "xchacha -> payload: {}", buffer_printer{payload});

                xor_nonce.assign(crypto::shorthash(shared_secret).span().first<SymmNonce::SIZE>());
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Failed to derive and decrypt path build message: {}", e.what());
                throw std::runtime_error{messages::ERROR_RESPONSE};
            }

            return ret;
        }

        std::tuple<HopID, SymmNonce, std::vector<std::byte>> deserialize_stream_hop(oxenc::bt_dict_consumer&& btdc)
        {
            std::tuple<HopID, SymmNonce, std::vector<std::byte>> ret;
            auto& [hop_id, nonce, payload] = ret;

            try
            {
                hop_id.assign(btdc.require_span<std::byte, HopID::SIZE>("h"));
                nonce.assign(btdc.require_span<std::byte, SymmNonce::SIZE>("n"));
                auto enc = btdc.require_span<std::byte>("x");
                payload.assign(enc.begin(), enc.end());
                btdc.finish();
                return ret;
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error{"Exception caught deserializing onion data: {}"_format(e.what())};
            }
        }

    }  // namespace ONION

    namespace PATH
    {
        namespace BUILD
        {

            const std::string NO_TRANSIT = messages::serialize_status_response("NOT ALLOWING TRANSIT"sv);
            const std::string BAD_LIFETIME = messages::serialize_status_response("BAD PATH LIFETIME (TOO LONG)"sv);
            const std::string BAD_FRAMES = messages::serialize_status_response("BAD FRAMES"sv);
            const std::string BAD_PATHID = messages::serialize_status_response("BAD PATH ID"sv);

        }  // namespace BUILD

        namespace CONTROL
        {
            /** Fields for transmitting Path Control:
                - 'e' : request endpoint being invoked
                - 'p' : request payload
            */
            std::vector<std::byte> serialize(std::string_view endpoint, std::span<const std::byte> payload)
            {
                oxenc::bt_dict_producer btdp;
                btdp.append("e", endpoint);
                btdp.append("p", payload);
                return to_bytes(btdp);
            }

            std::vector<std::byte> serialize_aligned(std::span<const std::byte> payload, const HopID& pivot_txid)
            {
                return serialize(
                    "path_control", ONION::serialize_stream_hop(pivot_txid, SymmNonce::make_random(), payload));
            }

            std::pair<std::string, std::string> deserialize(oxenc::bt_dict_consumer&& btdc)
            {
                std::pair<std::string, std::string> ret;
                auto& [endpoint, payload] = ret;

                try
                {
                    endpoint = btdc.require<std::string>("e");
                    payload = btdc.require<std::string>("p");
                    return ret;
                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error{"Exception caught deserializing path control: {}"_format(e.what())};
                }
            }
        }  // namespace CONTROL

        namespace DATA
        {
            /** Fields for transmitting Path Data:
                - 'i' : RouterID of sender
                - 'p' : messages payload
                NOTE: more fields may be added later as needed, hence the namespacing
            */
            std::string serialize(std::string payload, const RouterID& local)
            {
                oxenc::bt_dict_producer btdp;
                btdp.append("i", local.span());
                btdp.append("p", payload);
                return std::move(btdp).str();
            }

            std::pair<NetworkAddress, std::span<const std::byte>> deserialize(oxenc::bt_dict_consumer&& btdc)
            {
                std::pair<NetworkAddress, std::span<const std::byte>> ret;
                auto& [sender, payload] = ret;

                try
                {
                    sender = {RouterID{btdc.require_span<uint8_t, 32>("i")}, true};
                    payload = btdc.require_span<std::byte>("p");
                    return ret;
                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error{
                        "Exception caught deserializing outer datagram payload: {}"_format(e.what())};
                }
            }

            std::pair<HopID, std::string> deserialize_intermediate(oxenc::bt_dict_consumer&& btdc)
            {
                std::pair<HopID, std::string> ret;
                auto& [hop_id, payload] = ret;

                try
                {
                    hop_id.assign(btdc.require_span<std::byte, HopID::SIZE>("i"));
                    payload = btdc.require<std::string>("p");

                    return ret;
                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error{
                        "Exception caught deserializing intermediate datagram payload: {}"_format(e.what())};
                }
            }

            std::pair<session_tag, std::span<std::byte>> deserialize_inner(std::span<std::byte> payload)
            {
                std::pair<session_tag, std::span<std::byte>> ret;
                auto& [t, body] = ret;

                try
                {
                    if (payload.size() < session_tag::SIZE)
                        throw std::invalid_argument{"Deserialization failed: value is too short"};

                    t.assign(payload.first<session_tag::SIZE>());
                    body = payload.subspan<session_tag::SIZE>();
                    return ret;
                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error{
                        "Exception caught deserializing inner datagram payload: {}"_format(e.what())};
                }
            }
        }  // namespace DATA

    }  // namespace PATH

}  // namespace llarp
