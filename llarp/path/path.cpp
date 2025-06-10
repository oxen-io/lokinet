#include "path.hpp"

#include <llarp/messages/dht.hpp>
#include <llarp/messages/fetch.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/buffer.hpp>

#include <ranges>

namespace llarp::path
{
    static auto logcat = log::Cat("path");

    size_t Path::next_path_uuid = 0;

    Path::Path(Router& rtr, const std::vector<RemoteRC>& hop_rcs, std::weak_ptr<PathHandler> _handler)
        : handler{std::move(_handler)}, _router{rtr}, num_hops{hop_rcs.size()}, path_id{++next_path_uuid}
    {
        populate_internals(hop_rcs);
        log::trace(logcat, "Path successfully constructed -> {} :{}", to_string(), hop_string());
    }

    Path::~Path()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (is_linked())
            log::warning(logcat, "Path (ID:{}) destructed with {} linked sessions!", path_id, _linked_sessions.size());
    }

    void Path::populate_internals(const std::vector<RemoteRC>& hop_rcs)
    {
        hops.resize(num_hops);

        for (size_t i = 0; i < num_hops; ++i)
        {
            /** Conditions:
                - First hop RXID is unique, the rest are the previous hop TXID
                - Last hop upstream is it's own RID, the rest are the next hop RID
                - First hop downstream is client's RID, the rest are the previous hop RID
                - Local hop RXID is random, TXID is first hop RXID
                - Local hop upstream is first hop RID, downstream is local instance RID
            */

            hops[i]._rid = hop_rcs[i].router_id();
            hops[i]._txid = HopID::make_random();

            if (i == 0)
            {
                hops[i]._rxid = HopID::make_random();
                hops[i]._upstream = hop_rcs[i + 1].router_id();
                hops[i]._downstream = _router.local_rid();
            }
            else if (i == num_hops - 1)
            {
                hops[i]._rxid = hops[i - 1]._txid;
                hops[i]._upstream = hops[i]._rid;
                hops[i]._downstream = hops[i - 1]._rid;
            }
            else
            {
                hops[i]._rxid = hops[i - 1]._txid;
                hops[i]._upstream = hop_rcs[i + 1].router_id();
                hops[i]._downstream = hops[i - 1]._rid;
            }

            // generate dh kx components
            hops[i].kx = shared_kx_data::generate();

            // Conditions written as ternaries
            // hops[i]._rxid = i ? hops[i - 1]._txid : HopID::make_random();
            // hops[i]._upstream = i == num_hops - 1 ? hops[i]._rid : hop_rcs[i + 1].router_id();
            // hops[i]._downstream = i ? hops[i - 1]._rid : _router.local_rid();
        }

        hops.back().terminal_hop = true;

        log::trace(logcat, "Path populated with hops: {}", hop_string());

        // initialize parts of the clientintro
        intro.pivot_rid = hops.back().router_id();
        intro.pivot_txid = hops.back()._txid;

        log::trace(
            logcat, "Path client intro holding pivot_rid ({}) and pivot_txid ({})", intro.pivot_rid, intro.pivot_txid);
    }

    void Path::do_ping(std::chrono::milliseconds start_time)
    {
        if (!is_active()) return;

        log::trace(logcat, "Pinging path TXID={}", upstream_txid());
        send_path_control_message("path_ping"s, ""s, [self=get_weak(), start_time](oxen::quic::message m) {
                auto shared_self = self.lock();
                if (!shared_self) return;
                std::chrono::milliseconds now = llarp::time_now_ms();
                auto time_taken = now - start_time;
                if (m && m.body() == messages::OK_RESPONSE)
                {
                    log::trace(logcat, "Ping response for path TXID={} response received in {}", shared_self->upstream_txid(), time_taken);
                    shared_self->recent_ping_failures = 0;
                    shared_self->ping_average = std::chrono::milliseconds{((shared_self->ping_average * shared_self->ping_count) + time_taken) / ++shared_self->ping_count};
                }
                else
                {
                    log::debug(logcat, "Ping response for path TXID={} timed out in {}", shared_self->upstream_txid(), time_taken);
                    if (++shared_self->recent_ping_failures > 5)
                    {
                        log::debug(logcat, "Path TXID={} had too many ping timeouts, expiring.", shared_self->upstream_txid());
                        shared_self->intro.expiry = start_time;
                    }
                }
                });
    }

    void Path::link_session(session_tag t)
    {
        _linked_sessions.insert(t);
        log::trace(logcat, "Current path has {} linked sessions!", _linked_sessions.size());
    }

    bool Path::unlink_session(session_tag t)
    {
        auto n = _linked_sessions.erase(t);
        log::trace(logcat, "Current path has {} linked sessions!", _linked_sessions.size());
        return n != 0;
    }

    bool Path::operator<(const Path& other) const
    {
        auto& first_hop = hops.front();
        auto& other_first = other.hops.front();
        return std::tie(first_hop._txid, first_hop._rxid, first_hop._upstream)
            < std::tie(other_first._txid, other_first._rxid, other_first._upstream);
    }

    bool Path::operator==(const Path& other) const
    {
        if (hops.size() == other.hops.size())
            return std::ranges::equal(hops, other.hops);

        return false;
    }

    bool Path::operator!=(const Path& other) const { return not(*this == other); }

    bool Path::fetch_relay_contact(const RouterID& needed, bt_control_response_hook func)
    {
        return send_path_control_message("fetch_rcs", FetchRC::serialize(needed), std::move(func));
    }

    bool Path::find_client_contact(const hash_key& location, bt_control_response_hook func)
    {
        return send_path_control_message("find_cc", FindClientContact::serialize(location), std::move(func));
    }

    bool Path::publish_client_contact(const EncryptedClientContact& ecc, bt_control_response_hook func)
    {
        return send_path_control_message("publish_cc", PublishClientContact::serialize(ecc), std::move(func));
    }

    bool Path::resolve_sns(const std::string& name_hash, bt_control_response_hook func)
    {
        return send_path_control_message("resolve_sns", ResolveSNS::serialize(name_hash), std::move(func));
    }

    std::string Path::make_path_message(std::string inner_payload)
    {
        auto nonce = SymmNonce::make_random();

        for (const auto& hop : std::ranges::reverse_view(hops))
        {
            nonce = crypto::onion(
                reinterpret_cast<unsigned char*>(inner_payload.data()),
                inner_payload.size(),
                hop.kx.shared_secret,
                nonce,
                hop.kx.xor_nonce);
        }

        return ONION::serialize_hop(upstream_rxid().to_view(), nonce, std::move(inner_payload));
    }

    bool Path::send_path_data_message(std::string data)
    {
        auto payload = make_path_message(std::move(data));
        return _router.send_data_message(upstream_rid(), std::move(payload));
    }

    bool Path::send_path_control_message(std::string endpoint, std::string body, bt_control_response_hook func)
    {
        auto inner_payload = PATH::CONTROL::serialize(std::move(endpoint), std::move(body));
        auto outer_payload = make_path_message(std::move(inner_payload));
        return _router.send_control_message(upstream_rid(), "path_control", std::move(outer_payload), std::move(func));
    }

    bool Path::is_active(std::chrono::milliseconds now) const { return _is_established ? !is_expired(now) : false; }

    std::shared_ptr<PathHandler> Path::get_parent()
    {
        if (auto parent = handler.lock())
            return parent;

        return nullptr;
    }

    TransitHop Path::edge() const { return {hops.front()}; }

    RouterID Path::upstream_rid() { return hops.front().router_id(); }

    const RouterID& Path::upstream_rid() const { return hops.front().router_id(); }

    HopID Path::upstream_txid() { return hops.front().txid(); }

    const HopID& Path::upstream_txid() const { return hops.front().txid(); }

    HopID Path::upstream_rxid() { return hops.front().rxid(); }

    const HopID& Path::upstream_rxid() const { return hops.front().rxid(); }

    RouterID Path::pivot_rid() { return hops.back().router_id(); }

    const RouterID& Path::pivot_rid() const { return hops.back().router_id(); }

    HopID Path::pivot_txid() { return hops.back().txid(); }

    const HopID& Path::pivot_txid() const { return hops.back().txid(); }

    HopID Path::pivot_rxid() { return hops.back().rxid(); }

    const HopID& Path::pivot_rxid() const { return hops.back().rxid(); }

    std::string Path::to_string() const
    {
        return debug_string();
        // return "Path:[ Active:{} | Session-linked:{} | Local RID:{} | Pivot RID:{} | Edge RX:{} | Pivot TX:{}
        // ]"_format(
        //     detail::bool_alpha(is_active()),
        //     detail::bool_alpha(is_linked()),
        //     _router.local_rid().short_string(),
        //     pivot_rid().short_string(),
        //     upstream_rxid(),
        //     pivot_txid());
    }

    std::string Path::debug_string() const
    {
        return "Path:[ ID:{} | Pivot RID:{} | Edge RX:{} | Pivot TX:{} ]{}"_format(
            path_id, pivot_rid().short_string(), upstream_rxid(), pivot_txid(), hop_string());
    }

    handlers::SessionEndpoint& Path::parent() { return *_router.session_endpoint().get(); }

    std::string Path::hop_string() const
    {
        std::string hops_str;
        hops_str.reserve(hops.size() * 62);  // 52 for the pkey, 6 for .snode, 4 for the ' -> ' joiner
        for (const auto& hop : hops)
        {
            if (!hops.empty())
                hops_str += " -> ";
            hops_str += hop.router_id().short_string();
        }
        return hops_str;
    }

    nlohmann::json Path::ExtractStatus() const
    {
        auto now = llarp::time_now_ms();

        nlohmann::json obj{
            {"lastRecvMsg", to_json(last_recv_msg)},
            {"lastLatencyTest", to_json(last_latency_test)},
            {"expired", is_expired(now)},
            {"ready", is_active()},
        };

        std::vector<nlohmann::json> hopsObj;
        std::transform(hops.begin(), hops.end(), std::back_inserter(hopsObj), [](const auto& hop) -> nlohmann::json {
            return hop.ExtractStatus();
        });
        obj["hops"] = hopsObj;

        return obj;
    }

    void Path::Tick(std::chrono::milliseconds now)
    {
        if (not is_active())
            return;

        if (is_expired(now))
            return;
    }

    void Path::set_established()
    {
        log::trace(logcat, "Path marked as successfully established!");
        _is_established = true;
        intro.expiry = llarp::time_now_ms() + path::DEFAULT_LIFETIME;
    }

    bool Path::is_expired(std::chrono::milliseconds now) const { return intro.is_expired(now); }

    std::string Path::name() const { return "[ TX={} | RX={} ]"_format(upstream_txid(), upstream_rxid()); }

    template <typename Samples_t>
    static std::chrono::milliseconds computeLatency(const Samples_t& samps)
    {
        std::chrono::milliseconds mean = 0s;
        if (samps.empty())
            return mean;
        for (const auto& samp : samps)
            mean += samp;
        return mean / samps.size();
    }
}  // namespace llarp::path
