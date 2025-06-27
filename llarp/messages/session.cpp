#include "session.hpp"

#include "common.hpp"
#include "path.hpp"

#include <llarp/util/bspan.hpp>

namespace llarp
{
    static auto logcat = llarp::log::Cat("session.msgs");

    namespace InitiateSession
    {
        const std::string AUTH_ERROR = messages::serialize_status_response("AUTH ERROR");
        const std::string BAD_ROUTE = messages::serialize_status_response("BAD ROUTE");
        const std::string BAD_ADDRESS = messages::serialize_status_response("BAD ADDRESS");

        std::string serialize(
            const RouterID& local,
            HopID local_pivot_txid,
            HopID remote_pivot_txid,
            std::optional<std::string_view> auth_token,
            bool use_tun)
        {
            try
            {
                oxenc::bt_dict_producer btdp;

                btdp.append("i", local.span());
                btdp.append("p", local_pivot_txid.span());
                btdp.append("r", remote_pivot_txid.span());
                if (use_tun)
                    btdp.append("t", use_tun);
                // TOTHINK: this auth field
                if (auth_token)
                    btdp.append("u", *auth_token);

                return std::move(btdp).str();
            }
            catch (const std::exception& e)
            {
                log::error(logcat, "Exception caught encrypting session initiation message: {}", e.what());
                throw;
            }
        }

        std::pair<std::string, shared_kx_data> serialize_encrypt(
            const RouterID& local,
            const RouterID& remote,
            HopID local_pivot_txid,
            HopID remote_pivot_txid,
            std::optional<std::string_view> auth_token,
            bool use_tun)
        {
            try
            {
                std::string payload =
                    serialize(local, local_pivot_txid, remote_pivot_txid, std::move(auth_token), use_tun);

                auto kx_data = shared_kx_data::generate();

                kx_data.client_dh(remote);
                kx_data.encrypt(as_bspan(payload));
                kx_data.generate_xor();

                auto new_payload = ONION::serialize_hop(kx_data.pubkey, kx_data.nonce, as_bspan(payload));

                return {PATH::CONTROL::serialize("session_init", as_bspan(new_payload)), std::move(kx_data)};
            }
            catch (const std::exception& e)
            {
                log::error(logcat, "Exception caught encrypting session initiation message: {}", e.what());
                throw;
            }
        };

        std::tuple<NetworkAddress, HopID, HopID, bool, std::optional<std::string>> deserialize(
            oxenc::bt_dict_consumer&& btdc)
        {
            try
            {
                std::tuple<NetworkAddress, HopID, HopID, bool, std::optional<std::string>> result;
                auto& [initiator, local_pivot_txid, remote_pivot_txid, use_tun, maybe_auth] = result;

                RouterID init_rid;
                init_rid.assign(btdc.require_span<std::byte, RouterID::SIZE>("i"));
                initiator = {init_rid, true};
                remote_pivot_txid.assign(btdc.require_span<std::byte, HopID::SIZE>("p"));
                local_pivot_txid.assign(btdc.require_span<std::byte, HopID::SIZE>("r"));
                use_tun = btdc.maybe<bool>("t").value_or(false);
                maybe_auth = btdc.maybe<std::string>("u");

                return result;
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception caught decrypting session initiation message:{}", e.what());
                throw;
            }
        }

        std::tuple<shared_kx_data, NetworkAddress, HopID, HopID, bool, std::optional<std::string>> decrypt_deserialize(
            oxenc::bt_dict_consumer&& outer_btdc, const Ed25519SecretKey& local)
        {
            std::tuple<shared_kx_data, NetworkAddress, HopID, HopID, bool, std::optional<std::string>> result;
            auto& [kx_data, initiator, local_pivot_txid, remote_pivot_txid, use_tun, maybe_auth] = result;
            SymmNonce nonce;
            PubKey shared_pubkey;
            std::string payload;
            SharedSecret shared;

            try
            {
                std::tie(payload, kx_data) = ONION::deserialize_decrypt(std::move(outer_btdc), local);
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception caught deserializing/decrypting hop dict: {}", e.what());
                throw;
            }

            try
            {
                std::tie(initiator, local_pivot_txid, remote_pivot_txid, use_tun, maybe_auth) =
                    deserialize(oxenc::bt_dict_consumer{payload});
                return result;
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception caught decrypting session initiation message:{}", e.what());
                throw;
            }
        }

        std::string serialize_response(session_tag& t)
        {
            oxenc::bt_dict_producer btdp;
            btdp.append("t", t.view());
            return std::move(btdp).str();
        }

        session_tag deserialize_response(oxenc::bt_dict_consumer&& btdc)
        {
            try
            {
                return session_tag{btdc.require_span<std::byte, session_tag::SIZE>("t")};
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception caught deserializing session initiation response:{}", e.what());
                throw;
            }
        }
    }  // namespace InitiateSession

    namespace CloseSession
    {
        std::string serialize(session_tag& t)
        {
            oxenc::bt_dict_producer btdp;
            btdp.append("t", t.view());
            return std::move(btdp).str();
        }

        session_tag deserialize(oxenc::bt_dict_consumer&& btdc)
        {
            try
            {
                return session_tag{btdc.require_span<std::byte, session_tag::SIZE>("t")};
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception caught deserializing session close response:{}", e.what());
                throw;
            }
        }

    }  // namespace CloseSession

    namespace SetSessionTag
    {
        std::string serialize()
        {
            // FIXME TODO no values?  This doesn't seem right.
            oxenc::bt_dict_producer btdp;
            return std::move(btdp).str();
        }
    }  // namespace SetSessionTag

    namespace SessionPathSwitch
    {
        const std::string BAD_TAG = messages::serialize_status_response("BAD TAG");
        const std::string BAD_ID = messages::serialize_status_response("BAD ID");

        std::string serialize(session_tag t, HopID local_pivot_txid, HopID remote_pivot_txid)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append("p", local_pivot_txid.span());
            btdp.append("r", remote_pivot_txid.span());
            btdp.append("t", t.view());

            return std::move(btdp).str();
        }

        std::tuple<session_tag, HopID, HopID> deserialize(oxenc::bt_dict_consumer&& btdc)
        {
            std::tuple<session_tag, HopID, HopID> result;
            auto& [t, remote_pivot_txid, local_pivot_txid] = result;

            try
            {
                remote_pivot_txid.assign(btdc.require_span<std::byte, HopID::SIZE>("p"));
                local_pivot_txid.assign(btdc.require_span<std::byte, HopID::SIZE>("r"));
                t.assign(btdc.require_span<std::byte, session_tag::SIZE>("t"));
                return result;
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception caught deserializing PathSwitch message: {}", e.what());
                throw;
            }
        }

    }  // namespace SessionPathSwitch

}  // namespace llarp
