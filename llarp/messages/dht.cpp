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
            - 'n' : (only for network publishes, omitted for distribution through current inbound
              sessions). 0-3 position indicator.  A client publishes their introset to the 4 closest
              network locations by sending the introset down their inbound/utility paths to 4 random
              routers; each message containing a value from 0 to 3 indicating where to forward the
              CC to.

              E.g.
                client -> ...path1... -> relayA sends n=0 to ask relayA to forward to the best relay
                client -> ...path2... -> relayB sends n=1 to ask relayB to forward to the second-best relay

              and so on for n=2 and n=3.  The 4 publishing locations is for redundancy against
              publishing failures that might miss one, and against service node composition changes
              that might add or remove a service node (thus changing which 4 are the four best
              storage locations).
         */
        std::vector<std::byte> serialize(const EncryptedClientContact& ecc, std::optional<int> location)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append("e", ecc.bt_payload());
            btdp.append("n", location);

            return to_bytes(btdp);
        }

        std::pair<EncryptedClientContact, std::optional<int>> deserialize(oxenc::bt_dict_consumer&& btdc)
        {
            try
            {
                return {EncryptedClientContact{btdc.require_span<std::byte>("e")}, btdc.maybe<int>("n")};
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error{"Exception caught deserializing EncryptedClientContact: {}"_format(e.what())};
            }
        }
    }  // namespace PublishClientContact

    namespace FindClientContact
    {
        const std::string NOT_FOUND = messages::serialize_status_response("NOT FOUND");
        const std::string INSUFFICIENT = messages::serialize_status_response("INSUFFICIENT NODES");
        const std::string INVALID_ORDER = messages::serialize_status_response("INVALID ORDER");

        /** Bt-encoded contents:
            - 'k' : blinded pubkey corresponding to client contact

            Note: we are bt-encoding to leave space for future fields (ex: version)
         */
        std::vector<std::byte> serialize(const PubKey& location)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append("k", location.span());

            return to_bytes(btdp);
        }

        PubKey deserialize(oxenc::bt_dict_consumer&& btdc)
        {
            PubKey key;

            try
            {
                key.assign(btdc.require_span<std::byte, PubKey::SIZE>("k"));
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
        std::vector<std::byte> serialize_response(const EncryptedClientContact& ecc)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append("x", ecc.bt_payload());

            return to_bytes(btdp);
        }

        EncryptedClientContact deserialize_response(oxenc::bt_dict_consumer&& btdc)
        {
            EncryptedClientContact ecc;

            try
            {
                ecc = EncryptedClientContact{btdc.require_span<std::byte>("x")};
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
        std::vector<std::byte> serialize(std::span<const std::byte, SHORTHASHSIZE> name_hash)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append("s", name_hash);

            return to_bytes(btdp);
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
        std::vector<std::byte> serialize_response(const EncryptedSNSRecord& enc)
        {
            oxenc::bt_dict_producer btdp;

            btdp.append("x", enc.bt_payload());

            return to_bytes(btdp);
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
