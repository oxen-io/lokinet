#include "path.hpp"

#include "common.hpp"

#include <llarp/util/bspan.hpp>

namespace llarp
{

    static auto logcat = llarp::log::Cat("path.msgs");

    namespace ONION
    {
        std::string serialize_frames(const std::vector<std::string>& frames) { return oxenc::bt_serialize(std::move(frames)); }

        std::vector<std::string> deserialize_frames(std::string_view buf)
        {
            return oxenc::bt_deserialize<std::vector<std::string>>(buf);
        }

        /** Bt-encoded contents:
            - 'k' : Next upstream HopID (path messages) OR shared pubkey (path builds)
            - 'n' : Symmetric nonce used to encrypt the layer
            - 'x' : Encrypted payload transmitted to next recipient
        */
        std::string serialize_hop(
            std::span<const std::byte> key, const SymmNonce& nonce, std::span<const std::byte> encrypted)
        {
            oxenc::bt_dict_producer btdp;
            btdp.append("k", key);
            btdp.append("n", nonce.span());
            btdp.append("x", encrypted);

            return std::move(btdp).str();
        }

        std::pair<std::string, shared_kx_data> deserialize_decrypt(
            oxenc::bt_dict_consumer&& btdc, const Ed25519SecretKey& local_sk)
        {
            std::pair<std::string, shared_kx_data> ret;
            auto& [payload, kx_data] = ret;

            try
            {
                kx_data.pubkey.assign(btdc.require_span<std::byte, PubKey::SIZE>("k"));
                kx_data.nonce.assign(btdc.require_span<std::byte, SymmNonce::SIZE>("n"));
                payload = btdc.require<std::string>("x");
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception caught deserializing onion data: {}", e.what());
                throw std::runtime_error{messages::ERROR_RESPONSE};
            }

            log::trace(logcat, "payload: {}", buffer_printer{payload});

            try
            {
                kx_data.server_dh(local_sk);
                kx_data.decrypt(as_bspan(payload));

                log::trace(logcat, "xchacha -> payload: {}", buffer_printer{payload});

                kx_data.generate_xor();
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Failed to derive and decrypt outer wrapping!");
                throw std::runtime_error{messages::ERROR_RESPONSE};
            }

            return ret;
        }

        std::tuple<RouterID, SymmNonce, std::string> deserialize(oxenc::bt_dict_consumer&& btdc)
        {
            std::tuple<RouterID, SymmNonce, std::string> ret;
            auto& [rid, nonce, payload] = ret;

            try
            {
                rid.assign(btdc.require_span<std::byte, RouterID::SIZE>("k"));
                nonce.assign(btdc.require_span<std::byte, SymmNonce::SIZE>("n"));
                payload = btdc.require<std::string>("x");
                return ret;
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error{"Exception caught deserializing onion data: {}"_format(e.what())};
            }
        }

        std::tuple<HopID, SymmNonce, std::string> deserialize_hop(oxenc::bt_dict_consumer&& btdc)
        {
            std::tuple<HopID, SymmNonce, std::string> ret;
            auto& [hop_id, nonce, payload] = ret;

            try
            {
                hop_id.assign(btdc.require_span<std::byte, HopID::SIZE>("k"));
                nonce.assign(btdc.require_span<std::byte, SymmNonce::SIZE>("n"));
                payload = btdc.require<std::string>("x");
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
            const std::string BAD_CRYPTO = messages::serialize_status_response("BAD CRYPTO"sv);

            /** For each hop:
                - Generate an Ed keypair for the hop (`shared_key`)
                - Generate a symmetric nonce for subsequent DH
                - Derive the shared secret (`hop.shared`) for DH key-exchange using the Ed keypair, hop pubkey, and
                    symmetric nonce
                - Encrypt the hop info in-place using `hop.shared` and the generated symmetric nonce from DH
                - Generate the XOR nonce by hashing the symmetric key from DH (`hop.shared`) and truncating

                Bt-encoded contents:
                - 'k' : shared pubkey used to derive symmetric key
                - 'n' : symmetric nonce used for DH key-exchange
                - 'x' : encrypted payload
                    - 'r' : rxID (the path ID for messages going *to* the hop)
                    - 't' : txID (the path ID for messages coming *from* the client/path origin)
                    - 'u' : upstream hop RouterID

                All of these 'frames' are inserted sequentially into the list and padded with any needed dummy frames
            */
            std::string serialize_hop(path::TransitHop& hop)
            {
                auto hop_payload = hop.bt_encode();

                // client dh key derivation
                hop.kx.client_dh(hop.router_id());
                // encrypt payload
                hop.kx.encrypt(as_bspan(hop_payload));
                // generate nonceXOR value
                hop.kx.generate_xor();

                log::trace(
                    logcat,
                    "Hop serialized; nonce: {}, remote router_id: {}, shared pk: {}, shared secret: {}, payload: {}",
                    hop.kx.nonce,
                    hop.router_id(),
                    hop.kx.pubkey,
                    hop.kx.shared_secret,
                    buffer_printer{hop_payload});

                return ONION::serialize_hop(hop.kx.pubkey.span(), hop.kx.nonce, as_bspan(hop_payload));
            }

            std::shared_ptr<path::TransitHop> deserialize_hop(
                oxenc::bt_dict_consumer&& btdc, Router& r, const RouterID& src)
            {
                std::string payload;
                auto hop = std::make_shared<path::TransitHop>();

                try
                {
                    hop->kx.pubkey.assign(btdc.require_span<std::byte, PubKey::SIZE>("k"));
                    hop->kx.nonce.assign(btdc.require_span<std::byte, SymmNonce::SIZE>("n"));
                    payload = btdc.require<std::string_view>("x");
                }
                catch (const std::exception& e)
                {
                    log::warning(logcat, "Exception caught deserializing hop dict: {}", e.what());
                    throw;
                }

                log::trace(
                    logcat,
                    "Hop deserialized; nonce: {}, remote pk: {}, payload: {}",
                    hop->kx.nonce,
                    hop->kx.pubkey,
                    buffer_printer{payload});

                try
                {
                    hop->kx.server_dh(r.identity());
                    hop->kx.decrypt(as_bspan(payload));
                    hop->kx.generate_xor();

                    log::trace(
                        logcat,
                        "Hop decrypted; nonce: {}, remote pk: {}, payload: {}",
                        hop->kx.nonce,
                        hop->kx.pubkey,
                        buffer_printer{payload});

                    hop->deserialize(oxenc::bt_dict_consumer{std::move(payload)}, src, r);
                }
                catch (...)
                {
                    log::info(logcat, "Failed to derive and decrypt outer wrapping!");
                    throw std::runtime_error{BAD_CRYPTO};
                }

                log::trace(logcat, "TransitHop data successfully deserialized: {}", *hop);
                return hop;
            }
        }  // namespace BUILD

        namespace CONTROL
        {
            /** Fields for transmitting Path Control:
                - 'e' : request endpoint being invoked
                - 'p' : request payload
            */
            std::string serialize(std::string_view endpoint, std::span<const std::byte> payload)
            {
                oxenc::bt_dict_producer btdp;
                btdp.append("e", endpoint);
                btdp.append("p", payload);
                return std::move(btdp).str();
            }

            std::string serialize_aligned(std::span<const std::byte> payload, const HopID& pivot_txid)
            {
                auto pivot_payload = ONION::serialize_hop(pivot_txid, SymmNonce::make_random(), payload);
                return serialize("path_control", as_bspan(pivot_payload));
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

            std::string serialize_intermediate(
                const session_tag& tag, std::span<const std::byte> payload, const HopID& pivot_txid)
            {
                oxenc::bt_dict_producer btdp;
                btdp.append("i", pivot_txid.span());
                btdp.append_concat("p", tag.span(), payload);
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
