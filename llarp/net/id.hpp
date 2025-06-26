#pragma once
#include <fmt/format.h>

#include <string>

namespace llarp
{

    enum class NetID
    {
        MAINNET = 0,
        TESTNET = 1
    };

    inline std::string to_string(NetID n)
    {
        switch (n)
        {
            case NetID::MAINNET:
                return "lokinet";
            case NetID::TESTNET:
                return "testnet";
            default:
                return fmt::format("unknown network {}", static_cast<int>(n));
        }
    }

    NetID netid_from_string(std::string_view s)
    {
        if (s == "lokinet")
            return NetID::MAINNET;
        if (s == "testnet")
            return NetID::TESTNET;
        throw std::invalid_argument{"Invalid network id"};
    }

}  // namespace llarp

namespace fmt
{
    template <>
    struct formatter<llarp::NetID, char> : formatter<std::string>
    {
        template <typename FormatContext>
        auto format(llarp::NetID n, FormatContext& ctx) const
        {
            return formatter<std::string>::format(to_string(n), ctx);
        }
    };

}  // namespace fmt
