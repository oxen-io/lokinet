#include "session.hpp"

#include "common.hpp"
#include "path.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/util/bspan.hpp>

namespace llarp
{
    static auto logcat = llarp::log::Cat("session.msgs");

    namespace InitiateSession
    {
        const std::string AUTH_ERROR = messages::serialize_status_response("AUTH ERROR");
        const std::string BAD_ROUTE = messages::serialize_status_response("BAD ROUTE");
        const std::string BAD_ADDRESS = messages::serialize_status_response("BAD ADDRESS");

        std::vector<std::byte> serialize(
            const RouterID& local,
            HopID local_pivot_txid,
            HopID remote_pivot_txid,
            std::optional<std::string_view> auth_token)
        {
            try
            {
                oxenc::bt_dict_producer btdp;

                btdp.append("i", local.span());
                // TODO FIXME: include identity proof
                btdp.append("p", local_pivot_txid.span());
                btdp.append("r", remote_pivot_txid.span());
                // TOTHINK: this auth field
                if (auth_token)
                    btdp.append("u", *auth_token);

                return to_bytes(btdp);
            }
            catch (const std::exception& e)
            {
                log::error(logcat, "Exception caught encrypting session initiation message: {}", e.what());
                throw;
            }
        }

        std::pair<std::vector<std::byte>, SharedSecret> serialize_encrypt(
            const RouterID& local,
            const RouterID& remote,
            HopID local_pivot_txid,
            HopID remote_pivot_txid,
            std::optional<std::string_view> auth_token)
        {
            try
            {
                auto payload = serialize(local, local_pivot_txid, remote_pivot_txid, std::move(auth_token));

                auto [secret, eph_pk, dh_nonce] = crypto::dh_client_gen(remote);
                crypto::xchacha20(payload, secret, dh_nonce);

                oxenc::bt_dict_producer btdp;

                btdp.append("k", eph_pk.span());
                btdp.append("n", dh_nonce.span());
                btdp.append("x", payload);

                return {PATH::CONTROL::serialize("session_init", to_bytes(btdp)), secret};
            }
            catch (const std::exception& e)
            {
                log::error(logcat, "Exception caught encrypting session initiation message: {}", e.what());
                throw;
            }
        }

        Parameters decrypt_deserialize(oxenc::bt_dict_consumer&& outer_btdc, const Ed25519SecretKey& local)
        {
            Parameters ret;

            PubKey eph_pubkey;
            SymmNonce dh_nonce;
            std::vector<std::byte> payload;
            eph_pubkey.assign(outer_btdc.require_span<std::byte, PubKey::SIZE>("k"));
            dh_nonce.assign(outer_btdc.require_span<std::byte, SymmNonce::SIZE>("n"));
            auto payld = outer_btdc.require<std::span<const std::byte>>("x");
            payload.assign(payld.begin(), payld.end());
            outer_btdc.finish();

            crypto::dh_server(ret.session_key, eph_pubkey, local, dh_nonce);
            crypto::xchacha20(payload, ret.session_key, dh_nonce);

            oxenc::bt_dict_consumer inner{std::move(payload)};

            RouterID remote;
            remote.assign(inner.require_span<std::byte, RouterID::SIZE>("i"));
            ret.remote = {remote, true};

            // inverted from serialization order; their local is our remote and vice-versa
            ret.remote_pivot_txid.assign(inner.require_span<std::byte, HopID::SIZE>("p"));
            ret.local_pivot_txid.assign(inner.require_span<std::byte, HopID::SIZE>("r"));
            ret.auth_token = inner.maybe<std::string>("u");
            inner.finish();

            return ret;
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
