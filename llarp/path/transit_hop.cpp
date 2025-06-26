#include "transit_hop.hpp"

#include <llarp/messages/common.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/time.hpp>

namespace llarp::path
{
    static auto logcat = log::Cat("transit-hop");

    void TransitHop::deserialize(oxenc::bt_dict_consumer&& btdc, const RouterID& src, const Router& r)
    {
        try
        {
            bt_decode(std::move(btdc));
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "TransitHop caught bt parsing exception: {}", e.what());
            throw std::runtime_error{messages::ERROR_RESPONSE};
        }

        if (_rxid.is_zero() || _txid.is_zero())
            throw std::runtime_error{PATH::BUILD::BAD_PATHID};

        if (r.path_context.has_transit_hop(_rxid) || r.path_context.has_transit_hop(_txid))
            throw std::runtime_error{PATH::BUILD::BAD_PATHID};

        _downstream = src;

        if (_upstream == r.local_rid())
            terminal_hop = true;

        // generate hash of hop key for nonce mutation
        kx.generate_xor();

        log::trace(logcat, "TransitHop data successfully deserialized: {}", to_string());
    }

    void TransitHop::bt_decode(oxenc::bt_dict_consumer&& btdc)
    {
        _rxid.from_string(btdc.require<std::string_view>("r"));
        _txid.from_string(btdc.require<std::string_view>("t"));
        _upstream.from_string(btdc.require<std::string_view>("u"));
        expiry = llarp::time_now_ms() + path::DEFAULT_LIFETIME;
    }

    std::string TransitHop::bt_encode() const
    {
        oxenc::bt_dict_producer btdp;

        btdp.append("r", _rxid.to_view());
        btdp.append("t", _txid.to_view());
        btdp.append("u", _upstream.to_view());

        return std::move(btdp).str();
    }

    std::optional<std::pair<RouterID, HopID>> TransitHop::next_id(const HopID& h) const
    {
        std::optional<std::pair<RouterID, HopID>> ret = std::nullopt;

        if (h == _rxid)
            ret = {_upstream, _txid};
        else if (h == _txid)
            ret = {_downstream, _rxid};

        return ret;
    }

    nlohmann::json TransitHop::ExtractStatus() const
    {
        return {
            {"rid", router_id().ToHex()},
            {"rxid", rxid().ToHex()},
            {"txid", txid().ToHex()},
            {"expiry", to_json(expiry)},
            {"txid", _txid.ToHex()},
            {"rxid", _rxid.ToHex()}};
    }

    std::string TransitHop::to_string() const
    {
        return "TransitHop:[ Terminal:{} | TX:{} | RX:{} | Upstream:{} | Downstream:{} | Expiry:{} ]"_format(
            detail::bool_alpha(terminal_hop),
            _txid,
            _rxid,
            _upstream.short_string(),
            _downstream.short_string(),
            expiry.count());
    }

    SessionHop::SessionHop(const TransitHop& hop, handlers::SessionEndpoint& p) : TransitHop{hop}, _parent{p} {}

    void SessionHop::link_session(session_tag t)
    {
        _linked_sessions.insert(t);
        log::trace(logcat, "Current SessionHop has {} linked sessions!", _linked_sessions.size());
    }

    bool SessionHop::unlink_session(session_tag t)
    {
        auto n = _linked_sessions.erase(t);
        log::trace(logcat, "Current SessionHop has {} linked sessions!", _linked_sessions.size());
        return n != 0;
    }

    bool SessionHop::send_path_control_message(
        std::string method, std::string body, std::function<void(quic::message)> func)
    {
        auto inner_payload = PATH::CONTROL::serialize(std::move(method), std::move(body));
        return _parent._router.send_control_message(
            _downstream,
            "path_control",
            ONION::serialize_hop(_rxid.to_view(), SymmNonce::make_random(), std::move(inner_payload)),
            std::move(func));
    }

    bool SessionHop::send_path_data_message(std::string body)
    {
        auto nonce = SymmNonce::make_random() ^ kx.xor_nonce;
        crypto::onion(
            reinterpret_cast<unsigned char*>(body.data()), body.size(), kx.shared_secret, nonce, kx.xor_nonce);

        return _parent._router.send_data_message(
            _downstream, ONION::serialize_hop(_rxid.to_view(), nonce, std::move(body)));
    }

    std::string SessionHop::to_string() const
    {
        return "SessionHop:[ Num Sessions:{} | TX:{} | RX:{} | Upstream:{} | Downstream:{} | Expiry:{} ]"_format(
            _linked_sessions.size(),
            _txid,
            _rxid,
            _upstream.short_string(),
            _downstream.short_string(),
            expiry.count());
    }
}  // namespace llarp::path
