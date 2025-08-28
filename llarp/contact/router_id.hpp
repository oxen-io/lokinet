#pragma once

#include <llarp/crypto/keys.hpp>
#include <llarp/crypto/types.hpp>

#include <nlohmann/json_fwd.hpp>

namespace llarp
{
    using namespace std::literals;

    struct RouterID : public PubKey
    {
        using PubKey::PubKey;

        nlohmann::json ExtractStatus() const;

        std::string to_string() const;

        // will throw on failure!
        void from_network_address(std::string_view str);

        bool from_relay_address(std::string_view str);

        // Helper class that returns a fmt-printable address for a router ID on the fly.  This class
        // should only be used ephemerally.
        struct AddressPrinter
        {
            const RouterID& rid;
            bool is_relay;
            std::string to_string() const;
            static constexpr bool to_string_formattable = true;
        };
        AddressPrinter to_network_address(bool is_relay = true) const { return {.rid = *this, .is_relay = is_relay}; }
    };

    inline bool operator==(const RouterID& lhs, const RouterID& rhs) { return lhs.as_array() == rhs.as_array(); }
}  // namespace llarp

namespace std
{
    template <>
    struct hash<llarp::RouterID> : hash<llarp::PubKey>
    {};
}  // namespace std
