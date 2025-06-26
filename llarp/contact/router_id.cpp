#include "router_id.hpp"

#include <oxenc/base32z.h>

#include <iterator>

namespace llarp
{
    namespace
    {
        constexpr auto RELAY_DOT_TLD = ".snode"sv;
        constexpr auto CLIENT_DOT_TLD = ".loki"sv;
        constexpr auto B32Z_ID_SIZE = oxenc::to_base32z_size(RouterID::SIZE);
    }  // namespace

    std::string RouterID::to_network_address(bool is_relay) const
    {
        std::string r;
        r.reserve(B32Z_ID_SIZE + (is_relay ? RELAY_DOT_TLD : CLIENT_DOT_TLD).size());
        oxenc::to_base32z(begin(), end(), std::back_inserter(r));
        r += is_relay ? RELAY_DOT_TLD : CLIENT_DOT_TLD;
        return r;
    }

    std::string RouterID::to_string() const { return oxenc::to_base32z(begin(), end()); }

    nlohmann::json RouterID::ExtractStatus() const { return {{"snode", to_string()}, {"hex", ToHex()}}; }

    void RouterID::from_network_address(std::string_view str)
    {
        if (str.ends_with(RELAY_DOT_TLD))
            str.remove_suffix(RELAY_DOT_TLD.size());
        else if (str.ends_with(CLIENT_DOT_TLD))
            str.remove_suffix(CLIENT_DOT_TLD.size());
        else
            throw std::invalid_argument{
                "Did not find expected .loki or .snode TLD in network address '{}'"_format(str)};

        if (str.size() != B32Z_ID_SIZE || !oxenc::is_base32z(str) || !(str.back() == 'o' || str.back() == 'y'))
            throw std::invalid_argument{"RouterID input is incorrect (input: {})"_format(str)};

        oxenc::from_base32z(str.begin(), str.end(), begin());
    }

    bool RouterID::from_relay_address(std::string_view str)
    {
        if (!str.ends_with(RELAY_DOT_TLD))
            return false;
        str.remove_suffix(RELAY_DOT_TLD.size());
        if (str.size() != B32Z_ID_SIZE || !oxenc::is_base32z(str) || !(str.back() == 'o' || str.back() == 'y'))
            return false;
        oxenc::from_base32z(str.begin(), str.end(), begin());
        return true;
    }
}  // namespace llarp
