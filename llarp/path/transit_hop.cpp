#include "transit_hop.hpp"

#include "llarp/crypto/crypto.hpp"

#include <llarp/messages/common.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bspan.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/time.hpp>

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

    InboundRelayPath::InboundRelayPath(const TransitHop& hop, handlers::SessionEndpoint& p)
        : TransitHop{hop}, _parent{p}
    {}

    void InboundRelayPath::encrypt_path_message(std::vector<std::byte>& payload, SymmNonce&& nonce, std::byte type)
    {
        auto orig_size = payload.size();
        payload.resize(orig_size + Path::ENCRYPT_PATH_MESSAGE_OVERHEAD);
        static_assert(Path::ENCRYPT_PATH_MESSAGE_OVERHEAD == SymmNonce::SIZE + HopID::SIZE + 1);
        auto [inner_payload, bnonce, bhop, msgtype] = split_span_tail<SymmNonce::SIZE, HopID::SIZE, 1>(payload);
        assert(inner_payload.size() == orig_size);

        nonce ^= xor_nonce;
        crypto::xchacha20(inner_payload, shared_secret, nonce);
        nonce.copy_to(bnonce);
        rxid.copy_to(bhop);
        msgtype[0] = type;
    }

    void InboundRelayPath::send_path_control_message(
        std::string_view method,
        std::span<const std::byte> body,
        std::function<void(quic::message)> func,
        std::byte type)
    {
        auto payload = PATH::CONTROL::serialize(method, body);
        encrypt_path_message(payload, SymmNonce::make_random(), type);
        _parent.router.send_control_message(downstream, "path_control", std::move(payload), std::move(func));
    }

    void InboundRelayPath::send_path_data_message(std::vector<std::byte>&& body, SymmNonce&& nonce, std::byte type)
    {
        encrypt_path_message(body, std::move(nonce), type);
        _parent.router.send_data_message(downstream, std::move(body));
    }

    std::string InboundRelayPath::to_string() const
    {
        return "InboundRelayPath:[TX/RX:{}/{}; Up/Down:{}/{}; Exp:{}]"_format(
            txid, rxid, upstream.short_string(), downstream.short_string(), expiry.count());
    }
}  // namespace llarp::path
