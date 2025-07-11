#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace llarp
{
    using namespace std::literals;

    namespace TLD
    {
        inline constexpr auto SNODE = ".snode"sv;
        inline constexpr auto LOKI = ".loki"sv;

        inline constexpr bool allowed(std::string_view dot_tld) { return dot_tld == SNODE || dot_tld == LOKI; }
    }  //  namespace TLD

    namespace detail
    {
        std::optional<std::string> parse_addr_string(std::string_view arg, std::string_view tld);

        std::pair<std::string, uint16_t> parse_addr(std::string_view addr, std::optional<uint16_t> default_port);

    }  //  namespace detail

}  //  namespace llarp
