#include "session.hpp"

#include "common.hpp"
#include "path.hpp"

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

                btdp.append("i", local.to_view());
                btdp.append("p", local_pivot_txid.to_view());
                btdp.append("r", remote_pivot_txid.to_view());
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
                kx_data.encrypt(llarp::detail::to_uspan(payload));
                kx_data.generate_xor();

                auto new_payload = ONION::serialize_hop(kx_data.pubkey.to_view(), kx_data.nonce, std::move(payload));

                return {PATH::CONTROL::serialize("session_init", std::move(new_payload)), std::move(kx_data)};
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
                std::optional<std::string> maybe_auth = std::nullopt;

                RouterID init_rid;
                init_rid.from_string(btdc.require<std::string_view>("i"));
                NetworkAddress initiator{init_rid, true};
                HopID remote_pivot_txid;
                remote_pivot_txid.from_string(btdc.require<std::string_view>("p"));
                HopID local_pivot_txid;
                local_pivot_txid.from_string(btdc.require<std::string_view>("r"));
                bool use_tun = btdc.maybe<bool>("t").value_or(false);
                maybe_auth = btdc.maybe<std::string>("u");

                return {
                    std::move(initiator),
                    std::move(local_pivot_txid),
                    std::move(remote_pivot_txid),
                    use_tun,
                    std::move(maybe_auth)};
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
            SymmNonce nonce;
            PubKey shared_pubkey;
            std::string payload;
            SharedSecret shared;
            shared_kx_data kx_data{};

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
                auto [initiator, local_pivot_txid, remote_pivot_txid, use_tun, maybe_auth] =
                    deserialize(oxenc::bt_dict_consumer{payload});

                return {
                    std::move(kx_data),
                    std::move(initiator),
                    std::move(local_pivot_txid),
                    std::move(remote_pivot_txid),
                    use_tun,
                    std::move(maybe_auth)};
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
                session_tag tag;
                tag.read(btdc.require<std::string_view>("t"));
                return tag;
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
                session_tag tag;
                tag.read(btdc.require<std::string_view>("t"));
                return tag;
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

            btdp.append("p", local_pivot_txid.to_view());
            btdp.append("r", remote_pivot_txid.to_view());
            btdp.append("t", t.view());

            return std::move(btdp).str();
        }

        std::tuple<session_tag, HopID, HopID> deserialize(oxenc::bt_dict_consumer&& btdc)
        {
            session_tag t;
            HopID remote_pivot_txid, local_pivot_txid;

            try
            {
                remote_pivot_txid.from_string(btdc.require<std::string_view>("p"));
                local_pivot_txid.from_string(btdc.require<std::string_view>("r"));
                t.read(btdc.require<std::string_view>("t"));
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception caught deserializing PathSwitch message: {}", e.what());
                throw;
            }

            return {std::move(t), std::move(remote_pivot_txid), std::move(local_pivot_txid)};
        }

    }  // namespace SessionPathSwitch

}  // namespace llarp
