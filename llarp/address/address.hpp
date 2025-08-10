#pragma once

#include "utils.hpp"

#include <llarp/contact/keys.hpp>
#include <llarp/contact/router_id.hpp>
#include <llarp/contact/sns.hpp>
#include <llarp/util/aligned.hpp>

namespace llarp
{
    /// Combines a pubkey and client/snode flag to represent a generic (client or snode) address.
    struct NetworkAddress
    {
      private:
        RouterID _pubkey{};
        bool _is_client{false};

      public:
        NetworkAddress() = default;
        // Constructs from a full network address ending in '.loki' or '.snode' (but *not* an ONS
        // entry).  Throws std::invalid_argument if invalid.
        explicit NetworkAddress(std::string_view addr);
        // Constructs from a full network address (base32z-encoded pubkey) *not* ending in .loki or
        // .snode.  The client or snode status is determined by the bool.
        NetworkAddress(std::string_view addr, bool is_client);
        // Constructs from a pubkey and flag indicating whether this is a client (true) or snode
        // (false).
        NetworkAddress(const RouterID& rid, bool is_client) : _pubkey{rid}, _is_client{is_client} {}

        bool operator==(const NetworkAddress& other) const
        {
            return std::tie(_pubkey, _is_client) == std::tie(other._pubkey, other._is_client);
        }

        bool empty() const { return _pubkey.is_zero(); }

        bool client() const { return _is_client; }

        bool relay() const { return !_is_client; }

        const RouterID& router_id() const { return _pubkey; }

        // Returns a log proxy object that prints a shortened part of the pubkey:
        auto short_name() const { return _pubkey.short_string(); }

        std::string name() const { return _pubkey.to_string(); }

        std::string to_string() const { return name().append(_is_client ? TLD::LOKI : TLD::SNODE); }
        static constexpr bool to_string_formattable = true;
    };

}  // namespace llarp

namespace std
{
    template <>
    struct hash<llarp::NetworkAddress>
    {
        size_t operator()(const llarp::NetworkAddress& r) const { return llarp::AlignedHasher{}(r.router_id()); }
    };
}  //  namespace std
