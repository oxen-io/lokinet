#pragma once

#include "utils.hpp"

#include <llarp/contact/keys.hpp>
#include <llarp/contact/router_id.hpp>
#include <llarp/contact/sns.hpp>
#include <llarp/util/aligned.hpp>

#include <utility>

namespace llarp
{
    /** NOTE:
        - These classes are purposely differentiated at the moment. At first pass, they seem like they could be easily
            combined into one shared class utilizing some sort of variant or templating
        - This may eventually be true, but the currently enforced heterogeneity is intended to leave space for a
            near-future replacement of RouterID and PubKey with ClientKey and RelayKey
    */

    /** NetworkAddress:
        This address type conceptually encapsulates any addressible hidden service or exit node operating on the
        network. This type is to be strictly used in contexts referring to remote exit nodes or hidden services
        operated on clients and clients/relays respectively.
    */
    struct NetworkAddress
    {
      private:
        PubKey _pubkey{};
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
            return _pubkey == other._pubkey && _is_client == other._is_client;
        }

        bool is_empty() const { return _pubkey.is_zero(); }

        bool is_client() const { return _is_client; }

        bool is_relay() const { return !is_client(); }

        const PubKey& pubkey() const { return _pubkey; }

        PubKey& pubkey() { return _pubkey; }

        const RouterID& router_id() const { return static_cast<const RouterID&>(pubkey()); }

        RouterID& router_id() { return static_cast<RouterID&>(pubkey()); }

        std::string short_name() const { return _pubkey.short_string(); }

        std::string name() const { return _pubkey.to_string(); }

        std::string to_string() const { return name().append(_is_client ? TLD::LOKI : TLD::SNODE); }
        static constexpr bool to_string_formattable = true;
    };

    /** RelayAddress: Type that holds only a service node pubkey (unlike NetworkAddress, above,
     *   which can hold SN pubkey or client pubkey).
     */
    struct RelayAddress
    {
      private:
        PubKey _pubkey{};

      public:
        RelayAddress() = default;
        explicit RelayAddress(PubKey cpk) : _pubkey{std::move(cpk)} {}
        explicit RelayAddress(std::string_view addr);

        bool operator==(const RelayAddress& other) const;

        const PubKey& pubkey() const { return _pubkey; }

        PubKey& pubkey() { return _pubkey; }

        const RouterID& router_id() const { return static_cast<const RouterID&>(pubkey()); }

        RouterID& router_id() { return static_cast<RouterID&>(pubkey()); }

        std::string to_string() const { return _pubkey.to_string().append(TLD::SNODE); }
        static constexpr bool to_string_formattable = true;
    };

}  // namespace llarp

namespace std
{
    template <>
    struct hash<llarp::NetworkAddress>
    {
        size_t operator()(const llarp::NetworkAddress& r) const { return llarp::AlignedHasher{}(r.pubkey()); }
    };

    template <>
    struct hash<llarp::RelayAddress>
    {
        size_t operator()(const llarp::RelayAddress& r) const { return llarp::AlignedHasher{}(r.pubkey()); }
    };
}  //  namespace std
