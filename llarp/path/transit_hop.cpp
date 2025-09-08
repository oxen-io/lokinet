#include "transit_hop.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/link/endpoint.hpp>
#include <llarp/messages/common.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bspan.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/time.hpp>

#include <nlohmann/json.hpp>
#include <sodium/randombytes.h>

namespace llarp::path
{
    static auto logcat = log::Cat("transit-hop");

    TransitHopError::TransitHopError(std::string err_code)
        : std::runtime_error{"TransitHop construction failed: {}"_format(err_code)}, error_code{std::move(err_code)}
    {}

    std::optional<std::pair<RouterID, HopID>> TransitHop::next_id(const HopID& h) const
    {
        std::optional<std::pair<RouterID, HopID>> ret = std::nullopt;

        if (h == rxid)
            ret = {upstream, txid};
        else if (h == txid)
            ret = {downstream, rxid};

        return ret;
    }

    nlohmann::json TransitHop::ExtractStatus() const
    {
        return {
            {"rid", router_id.ToHex()}, {"rxid", rxid.ToHex()}, {"txid", txid.ToHex()}, {"expiry", to_json(expiry)}};
    }

    std::string TransitHop::to_string() const
    {
        return "TransitHop:[ Terminal:{} | TX:{} | RX:{} | Upstream:{} | Downstream:{} | Expiry:{} ]"_format(
            terminal_hop, txid, rxid, upstream.short_string(), downstream.short_string(), expiry.count());
    }

}  // namespace llarp::path
