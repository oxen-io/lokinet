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

#include <ranges>

namespace llarp::path
{
    static auto logcat = log::Cat("path");

    size_t Path::next_path_log_id = 0;

    Path::Path(Router& rtr, std::span<const RemoteRC> hop_rcs, PathHandler& handler)
        : handler{handler.weak_from_this()}, _router{rtr}, path_log_id{++next_path_log_id}
    {
        hops.resize(hop_rcs.size());

        for (size_t i = 0; i < hop_rcs.size(); ++i)
        {
            auto& hop = hops[i];
            hop.router_id = hop_rcs[i].router_id();
            hop.txid = HopID::make_random();
            // First hop RXID is unique, the rest are the previous hop TXID
            hop.rxid = i == 0 ? HopID::make_random() : hops[i - 1].txid;
            // Last hop upstream is it's own RID, the rest are the next hop RID
            hop.upstream = i == hop_rcs.size() - 1 ? hop.router_id : hop_rcs[i + 1].router_id();
            // First hop downstream is client's RID, the rest are the previous hop RID
            hop.downstream = i == 0 ? _router.local_rid() : hops[i - 1].router_id;

            // hop.shared_secret and hop.xor_nonce are not set yet: they get set via a call to
            // PathHandler::path_build_onion when we make the actual build path message (because
            // they also require generating and sending an ephemeral pubkey and dh nonce in the path
            // build message, which aren't required again once sent in that message).
        }

        hops.back().terminal_hop = true;

        log::trace(logcat, "Path populated with hops: {}", hop_string());

        // initialize parts of the clientintro
        intro.pivot_rid = hops.back().router_id;
        intro.pivot_txid = hops.back().txid;

        log::trace(
            logcat, "Path client intro holding pivot_rid ({}) and pivot_txid ({})", intro.pivot_rid, intro.pivot_txid);

        log::debug(logcat, "Path successfully constructed: {}", *this);
    }

    void Path::do_ping(std::chrono::milliseconds start_time)
    {
        if (!is_active())
            return;

        log::trace(logcat, "Pinging path TXID={}", edge().txid);
        send_path_control_message("path_ping", {}, [wself = weak_from_this(), start_time](quic::message m) {
            auto self = wself.lock();
            if (!self)
                return;
            std::chrono::milliseconds now = llarp::time_now_ms();
            auto time_taken = now - start_time;
            if (m && m.body() == messages::OK_RESPONSE)
            {
                log::trace(
                    logcat, "Ping response for path TXID={} response received in {}", self->edge().txid, time_taken);
                self->recent_ping_failures = 0;
                self->ping_average = std::chrono::milliseconds{
                    ((self->ping_average * self->ping_count) + time_taken) / ++self->ping_count};
            }
            else
            {
                log::debug(logcat, "Ping response for path TXID={} timed out in {}", self->edge().txid, time_taken);
                if (++self->recent_ping_failures > 5)
                {
                    log::debug(logcat, "Path TXID={} had too many ping timeouts, expiring.", self->edge().txid);
                    self->intro.expiry = start_time;
                }
            }
        });
    }

    bool Path::operator==(const Path& other) const { return hops == other.hops; }

    void Path::fetch_relay_contact(const RouterID& needed, std::function<void(quic::message)> func)
    {
        send_path_control_message("fetch_rcs", as_bspan(FetchRC::serialize({{needed}})), std::move(func));
    }

    void Path::find_client_contact(const hash_key& location, std::function<void(quic::message)> func)
    {
        send_path_control_message("find_cc", as_bspan(FindClientContact::serialize(location)), std::move(func));
    }

    void Path::publish_client_contact(const EncryptedClientContact& ecc, std::function<void(quic::message)> func)
    {
        send_path_control_message("publish_cc", as_bspan(PublishClientContact::serialize(ecc)), std::move(func));
    }

    void Path::resolve_sns(std::span<const std::byte, SHORTHASHSIZE> name_hash, std::function<void(quic::message)> func)
    {
        send_path_control_message("resolve_sns", as_bspan(ResolveSNS::serialize(name_hash)), std::move(func));
    }

    std::string Path::make_path_message(std::span<std::byte> inner_payload)
    {
        auto nonce = SymmNonce::make_random();

        for (const auto& hop : std::ranges::reverse_view(hops))
        {
            crypto::xchacha20(inner_payload, hop.shared_secret, nonce);
            nonce ^= hop.xor_nonce;
        }

        return ONION::serialize_hop(edge().rxid, nonce, std::move(inner_payload));
    }

    void Path::encrypt_path_message(std::vector<std::byte>& data, SymmNonce&& nonce)
    {
        auto& hopid = edge().rxid;
        data.resize(data.size() + PATH_DATA_MESSAGE_OVERHEAD);

        static_assert(sizeof(SymmNonce) == SymmNonce::SIZE);
        static_assert(sizeof(HopID) == HopID::SIZE);

        auto [inner_payload, bnonce, bhop, msgtype] = split_span_tail<SymmNonce::SIZE, HopID::SIZE, 1>(data);
        assert(msgtype.size() == 1);

        for (const auto& hop : std::ranges::reverse_view(hops))
        {
            crypto::xchacha20(inner_payload, hop.shared_secret, nonce);
            nonce ^= hop.xor_nonce;
        }

        nonce.copy_to(bnonce);
        hopid.copy_to(bhop);
        msgtype[0] = std::byte{0x01};
    }

    void Path::send_path_data_message(std::vector<std::byte>&& data, SymmNonce&& nonce)
    {
        encrypt_path_message(data, std::move(nonce));
        _router.send_data_message(edge().router_id, std::move(data));
    }

    void Path::send_path_control_message(
        std::string_view endpoint, std::span<const std::byte> body, std::function<void(quic::message)> func)
    {
        auto inner_payload = PATH::CONTROL::serialize(endpoint, body);
        std::vector<std::byte> payload;
        payload.reserve(inner_payload.size() + PATH_DATA_MESSAGE_OVERHEAD);
        payload.resize(inner_payload.size());
        std::memcpy(payload.data(), inner_payload.data(), inner_payload.size());
        encrypt_path_message(payload);
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

    void Path::set_established(std::chrono::milliseconds lifetime)
    {
        if (_is_established)
            return;

        log::trace(logcat, "Path marked as successfully established!");
        _is_established = true;
        // TODO FIXME: if we received this intro from the network then *altering* it here seems very
        // wrong:
        intro.expiry = llarp::time_now_ms() + lifetime;
    }

    std::string Path::name() const { return "[ TX={} | RX={} ]"_format(edge().txid, edge().rxid); }

}  // namespace llarp::path
