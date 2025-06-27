#pragma once

#include <llarp/contact/keys.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/util/formattable.hpp>

#include <nlohmann/json.hpp>

namespace llarp
{
    struct RouterID : public PubKey
    {
        using PubKey::PubKey;

        nlohmann::json ExtractStatus() const;

        std::string to_string() const;

        std::string to_network_address(bool is_relay = true) const;

        // will throw on failure!
        void from_network_address(std::string_view str);

        bool from_relay_address(std::string_view str);
    };

    inline bool operator==(const RouterID& lhs, const RouterID& rhs) { return lhs.as_array() == rhs.as_array(); }
}  // namespace llarp

namespace std
{
    template <>
    struct hash<llarp::RouterID> : hash<llarp::PubKey>
    {};
}  // namespace std
