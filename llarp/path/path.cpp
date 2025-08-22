#include "path.hpp"

#include "llarp/crypto/crypto.hpp"
#include "path_handler.hpp"

#include <llarp/messages/dht.hpp>
#include <llarp/messages/fetch.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bspan.hpp>
#include <llarp/util/buffer.hpp>

#include <chrono>
#include <ranges>

namespace llarp::path
{
    static auto logcat = log::Cat("path");

    size_t Path::next_path_log_id = 0;

    Path::Path(
        Router& rtr, std::span<const RemoteRC> hop_rcs, PathHandler& handler, std::chrono::milliseconds expiry_ts)
        : handler{handler.weak_from_this()}, _router{rtr}, _expiry{expiry_ts}, path_log_id{++next_path_log_id}
    {
        hops.resize(hop_rcs.size());

        for (size_t i = 0; i < hop_rcs.size(); ++i)
        {
            const bool last = i + 1 == hop_rcs.size();
            auto& hop = hops[i];
            hop.router_id = hop_rcs[i].router_id();
            // First hop RXID is unique, the rest are the previous hop TXID
            hop.rxid = i == 0 ? HopID::make_random() : hops[i - 1].txid;
            // Pivot hop TXID is not useful, and so is simply set equal to the pivot RXID.
            hop.txid = last ? hop.rxid : HopID::make_random();
            // Last hop upstream is it's own RID, the rest are the next hop RID
            hop.upstream = last ? hop.router_id : hop_rcs[i + 1].router_id();
            // First hop downstream is client's RID, the rest are the previous hop RID
            hop.downstream = i == 0 ? _router.local_rid() : hops[i - 1].router_id;

            // hop.shared_secret and hop.xor_nonce are not set yet: they get set via a call to
            // PathHandler::path_build_onion when we make the actual build path message (because
            // they also require generating and sending an ephemeral pubkey and dh nonce in the path
            // build message, which aren't required again once sent in that message).
        }

        hops.back().terminal_hop = true;

        log::trace(logcat, "Path populated with hops: {}", hop_string());

        log::debug(logcat, "Path successfully constructed: {}", *this);
    }

    ClientIntro Path::make_intro() const
    {
        ClientIntro intro;
        intro.relay = hops.back().router_id;
        intro.hop = hops.back().txid;
        intro.expiry = std::chrono::sys_seconds{std::chrono::floor<std::chrono::seconds>(_expiry)};
        return intro;
    }

    std::string Path::ping_stats_printer::to_string() const
    {
        if (p.ping_responses == 0)
            return "0.0%";

        double mean = (double)p.ping_cumulative.count() / p.ping_responses;
        double success_pct = p.ping_responses / (double)(p.ping_responses + p.ping_timeouts) * 100.0;
        if (p.ping_responses == 1)
            return "{:.1f}%, {:.0f}ms avg"_format(success_pct, mean);

        double sd = std::sqrt(((double)p.ping_sq_cumulative - p.ping_responses * mean * mean) / (p.ping_responses - 1));
        return "{:.1f}%, {:.0f}ms avg, {:.1f}ms s.d."_format(success_pct, mean, sd);
    }

    void Path::do_ping(std::chrono::milliseconds start_time)
    {
        if (!is_active())
            return;

        log::trace(logcat, "Pinging path TXID={}", edge().txid);
        send_path_control_message("path_ping", {}, [this, wself = weak_from_this(), start_time](quic::message m) {
            auto sself = wself.lock();
            if (!sself)
                return;
            std::chrono::milliseconds now = llarp::time_now_ms();
            auto time_taken = now - start_time;
            if (m)
            {
                ping_responses++;
                ping_recent_timeouts = 0;
                ping_cumulative += time_taken;
                ping_sq_cumulative += time_taken.count() * time_taken.count();

                if (m.body() == messages::OK_RESPONSE)
                    log::debug(
                        logcat,
                        "Ping response for path {} (txid={}) response received in {} ({})",
                        *this,
                        edge().txid,
                        time_taken,
                        printable_ping_stats());
                else
                    log::warning(
                        logcat,
                        "Path {} ping was successful (in {}) but had unexpected response body: {}",
                        *this,
                        time_taken,
                        buffer_printer(m.body()));
            }
            else
            {
                bool expire = true;
                if (m.timed_out)
                {
                    ping_timeouts++;
                    log::debug(
                        logcat,
                        "Ping response for path {} (txid={}) timed out after {} ({})",
                        *this,
                        edge().txid,
                        time_taken,
                        printable_ping_stats());
                    expire = ++ping_recent_timeouts > 5;
                    if (expire)
                        log::warning(
                            logcat,
                            "Path {} (txid={}) had too many ping timeouts ({}); expiring path.",
                            *this,
                            edge().txid,
                            ping_recent_timeouts);
                }
                else
                    log::warning(
                        logcat,
                        "{} path_ping returned a path error (in {}): {}",
                        *this,
                        time_taken,
                        buffer_printer(m.body()));

                if (expire)
                    _expiry = start_time;
            }
        });
    }

    bool Path::operator==(const Path& other) const { return hops == other.hops; }

    void Path::fetch_relay_contact(const RouterID& needed, std::function<void(quic::message)> func)
    {
        send_path_control_message("fetch_rcs", FetchRC::serialize({&needed, 1}), std::move(func));
    }

    void Path::fetch_relay_contacts(std::span<const RouterID> needed, std::function<void(quic::message)> func)
    {
        send_path_control_message("fetch_rcs", FetchRC::serialize(needed), std::move(func));
    }

    void Path::find_client_contact(const hash_key& location, std::function<void(quic::message)> func)
    {
        send_path_control_message("find_cc", FindClientContact::serialize(location), std::move(func));
    }

    void Path::publish_client_contact(const EncryptedClientContact& ecc, std::function<void(quic::message)> func)
    {
        send_path_control_message("publish_cc", PublishClientContact::serialize(ecc), std::move(func));
    }

    void Path::resolve_sns(std::span<const std::byte, SHORTHASHSIZE> name_hash, std::function<void(quic::message)> func)
    {
        send_path_control_message("resolve_sns", ResolveSNS::serialize(name_hash), std::move(func));
    }

    void Path::encrypt_path_message(std::vector<std::byte>& data, SymmNonce&& nonce, std::byte type)
    {
        auto& hopid = edge().rxid;
        auto inner_size = data.size();
        data.resize(inner_size + ENCRYPT_PATH_MESSAGE_OVERHEAD);

        static_assert(sizeof(SymmNonce) == SymmNonce::SIZE);
        static_assert(sizeof(HopID) == HopID::SIZE);

        auto [inner_payload, bnonce, bhop, msgtype] = split_span_tail<SymmNonce::SIZE, HopID::SIZE, 1>(data);
        assert(inner_payload.size() == inner_size);

        for (const auto& hop : std::ranges::reverse_view(hops))
        {
            crypto::xchacha20(inner_payload, hop.shared_secret, nonce);
            nonce ^= hop.xor_nonce;
        }

        nonce.copy_to(bnonce);
        hopid.copy_to(bhop);
        msgtype[0] = type;
    }

    void Path::send_path_data_message(std::vector<std::byte>&& data, SymmNonce&& nonce, std::byte type)
    {
        encrypt_path_message(data, std::move(nonce), type);
        _router.send_data_message(edge().router_id, std::move(data));
    }

    void Path::send_path_control_message(
        std::string_view endpoint,
        std::span<const std::byte> body,
        std::function<void(quic::message)> func,
        std::byte type)
    {
        auto inner_payload = PATH::CONTROL::serialize(endpoint, body);
        std::vector<std::byte> payload;
        payload.reserve(inner_payload.size() + ENCRYPT_PATH_MESSAGE_OVERHEAD);
        payload.resize(inner_payload.size());
        std::memcpy(payload.data(), inner_payload.data(), inner_payload.size());
        encrypt_path_message(payload, SymmNonce::make_random(), type);
        _router.send_control_message(edge().router_id, "path_control", std::move(payload), std::move(func));
    }

    std::string Path::to_string() const { return "Path{{{}}}[{}]"_format(path_log_id, hop_string()); }

    std::string path_hop_stringifier::to_string() const
    {
        return fmt::to_string(
            fmt::join(hops | std::views::transform([](auto& h) { return h.router_id.short_string(); }), "⟷"));
    }
    path_hop_stringifier Path::hop_string() const { return {hops}; }

    nlohmann::json Path::ExtractStatus() const
    {
        auto now = llarp::time_now_ms();

        nlohmann::json obj{
            {"lastRecvMsg", to_json(last_recv_msg)},
            {"lastLatencyTest", to_json(last_latency_test)},
            {"expired", is_expired(now)},
            {"ready", is_active()},
        };

        auto json_hops = nlohmann::json::array();
        for (const auto& hop : hops)
            json_hops.push_back(hop.ExtractStatus());
        obj["hops"] = std::move(json_hops);

        return obj;
    }

    void Path::set_established()
    {
        if (_is_established)
            return;

        log::trace(logcat, "Path marked as successfully established!");
        _is_established = true;
    }

    std::string Path::name() const { return "[ TX={} | RX={} ]"_format(edge().txid, edge().rxid); }

}  // namespace llarp::path
