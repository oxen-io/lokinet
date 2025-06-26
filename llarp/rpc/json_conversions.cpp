#include "json_conversions.hpp"

#include <nlohmann/json.hpp>

namespace llarp
{
    static auto logcat = log::Cat("RPC-conversions");

    void to_json(nlohmann::json& j, const ipv4_net& ipr) { j = ipr.to_string(); }

    void from_json(const nlohmann::json& j, ipv4_net& ipr)
    {
        try
        {
            ipr = parse_ipv4_net(j.get<std::string_view>());
        }
        catch (const std::exception& e)
        {
            log::error(logcat, "Failed to parse ipv4 network from json: {}", e.what());
            throw;
        }
    }

}  // namespace llarp
