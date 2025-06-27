#pragma once

#include "common.hpp"

#include <llarp/contact/client_contact.hpp>
#include <llarp/contact/sns.hpp>

namespace llarp
{
    namespace PublishClientContact
    {
        extern const std::string INVALID;
        extern const std::string EXPIRED;

        std::string serialize(const EncryptedClientContact& ecc, std::optional<RouterID> remote = std::nullopt);

        std::pair<EncryptedClientContact, std::optional<RouterID>> deserialize(oxenc::bt_dict_consumer&& btdc);

    }  // namespace PublishClientContact

    namespace FindClientContact
    {
        extern const std::string NOT_FOUND;
        extern const std::string INSUFFICIENT;
        extern const std::string INVALID_ORDER;

        /** Bt-encoded contents:
            - 'k' : DHT key corresponding to client contact

            Note: we are bt-encoding to leave space for future fields (ex: version)
         */
        std::string serialize(const hash_key& location);

        hash_key deserialize(oxenc::bt_dict_consumer&& btdc);

        /** Bt-encoded contents:
            - 'x' : EncryptedClientContact

            Note: we are bt-encoding to leave space for future fields (ex: version)
         */
        std::string serialize_response(const EncryptedClientContact& ecc);

        EncryptedClientContact deserialize_response(oxenc::bt_dict_consumer&& btdc);

    }  //  namespace FindClientContact

    namespace ResolveSNS
    {
        extern const std::string NOT_FOUND;

        /** Bt-encoded contents:
            - 's' : SNS name

            Note: we are bt-encoding to leave space for future fields (ex: version)
         */
        std::string serialize(std::span<const std::byte, SHORTHASHSIZE> name_hash);

        std::string deserialize(oxenc::bt_dict_consumer&& btdc);

        /** Bt-encoded contents:
            - 'x' : EncryptedSNSRecord

            Note: we are bt-encoding to leave space for future fields (ex: version)
         */
        std::string serialize_response(const EncryptedSNSRecord& enc);

        EncryptedSNSRecord deserialize_response(oxenc::bt_dict_consumer&& btdc);

    }  // namespace ResolveSNS

}  // namespace llarp
