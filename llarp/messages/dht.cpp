#include "dht.hpp"

namespace llarp
{
    static auto logcat = log::Cat("dht.msgs");

    namespace PublishClientContact
    {
        const std::string INVALID = messages::serialize_status_response("INVALID CC");
        const std::string EXPIRED = messages::serialize_status_response("EXPIRED CC");

        /** Bt-encoded contents:
            - 'e' : EncryptedClientContact
            - 'i' : (Optional) RouterID of dispatching client, only sent on session paths

            Note: we are bt-encoding to leave space for future fields (ex: version)
         */
        std::string serialize(const EncryptedClientContact& ecc, std::optional<RouterID> remote)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append("e", ecc.bt_payload());
            if (remote.has_value())
                btdp.append("i", remote->to_view());

            return std::move(btdp).str();
        }

        std::pair<EncryptedClientContact, std::optional<RouterID>> deserialize(oxenc::bt_dict_consumer&& btdc)
        {
            std::pair<EncryptedClientContact, std::optional<RouterID>> ret;
            auto& [ecc, sender] = ret;

            try
            {
                ecc = EncryptedClientContact{btdc.require<std::string_view>("e")};

                if (btdc.skip_until("i"))
                    sender.emplace(btdc.consume_span<std::byte, 32>());
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error{"Exception caught deserializing EncryptedClientContact: {}"_format(e.what())};
            }

            return ret;
        }
    }  // namespace PublishClientContact

    namespace FindClientContact
    {
        const std::string NOT_FOUND = messages::serialize_status_response("NOT FOUND");
        const std::string INSUFFICIENT = messages::serialize_status_response("INSUFFICIENT NODES");
        const std::string INVALID_ORDER = messages::serialize_status_response("INVALID ORDER");

        /** Bt-encoded contents:
            - 'k' : DHT key corresponding to client contact

            Note: we are bt-encoding to leave space for future fields (ex: version)
         */
        std::string serialize(const hash_key& location)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append("k", location.to_view());

            return std::move(btdp).str();
        }

        hash_key deserialize(oxenc::bt_dict_consumer&& btdc)
        {
            hash_key key;

            try
            {
                key.from_string(btdc.require<std::string_view>("k"));
            }
            catch (const std::exception& e)
            {
                log::error(logcat, "Error: failed to deserialize FindClientContact contents: {}", e.what());
                throw;
            }

            return key;
        }

        /** Bt-encoded contents:
            - 'x' : EncryptedClientContact

            Note: we are bt-encoding to leave space for future fields (ex: version)
         */
        std::string serialize_response(const EncryptedClientContact& ecc)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append("x", ecc.bt_payload());

            return std::move(btdp).str();
        }

        EncryptedClientContact deserialize_response(oxenc::bt_dict_consumer&& btdc)
        {
            EncryptedClientContact ecc;

            try
            {
                ecc = EncryptedClientContact{btdc.require<std::string_view>("x")};
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error{"Exception caught deserializing EncryptedClientContact: {}"_format(e.what())};
            }

            return ecc;
        }
    }  //  namespace FindClientContact

    namespace ResolveSNS
    {
        const std::string NOT_FOUND = messages::serialize_status_response("NOT FOUND");

        /** Bt-encoded contents:
            - 's' : SNS name

            Note: we are bt-encoding to leave space for future fields (ex: version)
         */
        std::string serialize(const std::string& name_hash)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append("s", name_hash);

            return std::move(btdp).str();
        }

        std::string deserialize(oxenc::bt_dict_consumer&& btdc)
        {
            try
            {
                return btdc.require<std::string>("s");
            }
            catch (const std::exception& e)
            {
                log::error(logcat, "Error: failed to deserialize ResolveSNS contents: {}", e.what());
                throw;
            }
        }

        /** Bt-encoded contents:
            - 'x' : EncryptedSNSRecord

            Note: we are bt-encoding to leave space for future fields (ex: version)
         */
        std::string serialize_response(const EncryptedSNSRecord& enc)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append("x", enc.bt_payload());

            return std::move(btdp).str();
        }

        EncryptedSNSRecord deserialize_response(oxenc::bt_dict_consumer&& btdc)
        {
            EncryptedSNSRecord enc{};

            try
            {
                enc = EncryptedSNSRecord::deserialize(btdc.require<std::string_view>("x"));
            }
            catch (const std::exception& e)
            {
                log::error(logcat, "Error: failed to deserialize ResolveSNS contents: {}", e.what());
                throw;
            }

            return enc;
        }
    }  // namespace ResolveSNS

}  // namespace llarp
