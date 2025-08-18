#pragma once

#include <llarp/address/address.hpp>
#include <llarp/auth/auth.hpp>
#include <llarp/path/path_types.hpp>
#include <llarp/util/logging/buffer.hpp>

namespace llarp
{
    /** Fields for initiating sessions:
        - 'k' : ephemeral pubkey used to derive shared secret
        - 'n' : nonce used for key exchange 
        - 'x' : encrypted payload
            - 'i' : RouterID of initiator
            - 'p' : HopID at the pivot taken from local ClientIntro
            - 'r' : HopID at the pivot taken from remote's ClientIntro
            - 'u' : Authentication field
                - bt-encoded dict, values TBD
    */
    namespace InitiateSession
    {
        extern const std::string AUTH_ERROR;
        extern const std::string BAD_ROUTE;
        extern const std::string BAD_ADDRESS;

        std::vector<std::byte> serialize(
            const RouterID& local,
            HopID local_pivot_txid,
            HopID remote_pivot_txid,
            std::optional<std::string_view> auth_token);

        std::pair<std::vector<std::byte>, SharedSecret> serialize_encrypt(
            const RouterID& local,
            const RouterID& remote,
            HopID local_pivot_txid,
            HopID remote_pivot_txid,
            std::optional<std::string_view> auth_token);

        struct Parameters {
            // FIXME: need some signature to prove remote owns this pubkey
            NetworkAddress remote;
            HopID local_pivot_txid;
            HopID remote_pivot_txid;
            SharedSecret session_key;
            std::optional<std::string> auth_token;
            // FIXME: need client's session tag here once that's implemented
        };

        Parameters decrypt_deserialize(
            oxenc::bt_dict_consumer&& outer_btdc, const Ed25519SecretKey& local);

        std::string serialize_response(session_tag& t);

        session_tag deserialize_response(oxenc::bt_dict_consumer&& btdc);

    }  // namespace InitiateSession

    namespace CloseSession
    {
        std::string serialize(session_tag& t);

        session_tag deserialize(oxenc::bt_dict_consumer&& btdc);

    }  // namespace CloseSession

    /** Fields for setting a session tag:
     */
    namespace SetSessionTag
    {
        std::string serialize();

    }  // namespace SetSessionTag

    /** Fields for switching session paths:
        - 'p' : HopID at the pivot taken from local ClientIntro
        - 'r' : HopID at the pivot taken from remote's ClientIntro
        - 't' : session_tag for current session
    */
    namespace SessionPathSwitch
    {
        extern const std::string BAD_TAG;
        extern const std::string BAD_ID;

        std::string serialize(session_tag t, HopID local_pivot_txid, HopID remote_pivot_txid);

        std::tuple<session_tag, HopID, HopID> deserialize(oxenc::bt_dict_consumer&& btdc);

    }  // namespace SessionPathSwitch

}  // namespace llarp
