#include "relay_contact.hpp"

#include <llarp/constants/version.hpp>
#include <llarp/util/logging.hpp>

#include <oxenc/bt_producer.h>
#include <oxenc/bt_serialize.h>

#include <chrono>
#include <unordered_set>

namespace llarp
{
    static auto logcat = log::Cat("relay-contact");

    void RelayContact::load(NetID netid, bool accept_expired)
    {
        oxenc::bt_dict_consumer btdc{_payload};
        if (auto rc_ver = btdc.require<uint8_t>(""); rc_ver != RelayContact::VERSION)
            throw std::runtime_error{"Invalid RC: do not know how to parse v{} RCs"_format(rc_ver)};

        auto parsed_netid = static_cast<NetID>(btdc.maybe<int>("#").value_or(static_cast<int>(NetID::MAINNET)));

        if (netid != parsed_netid)
            throw std::runtime_error{
                "Invalid RC netid: expected {}, got {}; this is an RC for a different network!"_format(
                    netid, parsed_netid)};
        _netid = netid;

        auto ipv4_port = btdc.require<std::string_view>("4");

        if (ipv4_port.size() != 6)
            throw std::runtime_error{
                "Invalid RC address: expected 6-byte IPv4 IP/port, got {}"_format(ipv4_port.size())};

        sockaddr_in s4;
        s4.sin_family = AF_INET;

        std::memcpy(&s4.sin_addr.s_addr, ipv4_port.data(), 4);
        std::memcpy(&s4.sin_port, ipv4_port.data() + 4, 2);

        _addr = quic::Address{&s4};

        if (!_addr.is_public() and BLOCK_BOGONS)
            throw std::runtime_error{"Invalid RC: IPv4 address is not a publicly routable IP"};

        if (auto ipv6_port = btdc.maybe<std::string_view>("6"))
        {
            if (ipv6_port->size() != 18)
                throw std::runtime_error{
                    "Invalid RC address: expected 18-byte IPv6 IP/port, got {}"_format(ipv6_port->size())};

            sockaddr_in6 s6{};
            s6.sin6_family = AF_INET6;

            std::memcpy(&s6.sin6_addr.s6_addr, ipv6_port->data(), 16);
            std::memcpy(&s6.sin6_port, ipv6_port->data() + 16, 2);

            _addr6.emplace(&s6);
            if (!_addr6->is_public())
                throw std::runtime_error{"Invalid RC: IPv6 address is not a publicly routable IP"};
        }
        else
        {
            _addr6.reset();
        }

        auto pubkey = btdc.require<std::string_view>("p");
        if (pubkey.size() != 32)
            throw std::runtime_error{"Invalid RC pubkey: expected 32 bytes, got {}"_format(pubkey.size())};
        std::memcpy(_router_id.data(), pubkey.data(), 32);

        _timestamp = std::chrono::sys_seconds{std::chrono::seconds{btdc.require<uint64_t>("t")}};

        auto ver = btdc.require<std::span<const uint8_t>>("v");

        if (ver.size() != 3)
            throw std::runtime_error{"Invalid RC router version: received {} bytes, expected 3"_format(ver.size())};

        for (int i = 0; i < 3; i++)
            _router_version[i] = ver[i];

        btdc.require_signature(
            "~", [this, accept_expired](std::span<const std::byte> msg, std::span<const std::byte> sig) {
                if (sig.size() != 64)
                    throw std::runtime_error{"Invalid signature: not 64 bytes"};

                if (!accept_expired and is_expired(time_now_ms()))
                    throw std::runtime_error{"Rejecting expired RemoteRC!"};

                if (not crypto::verify(router_id(), msg, sig.first<64>()))
                    throw std::runtime_error{"Failed to verify RemoteRC signature"};
            });

        if (not btdc.is_finished())
            throw std::runtime_error{"RemoteRC has invalid trailing fields"};

        btdc.finish();
    }

    bool RelayContact::write(const fs::path& fname) const
    {
        try
        {
            util::buffer_to_file(fname, _payload);
        }
        catch (const std::exception& e)
        {
            log::error(logcat, "Failed to write RC to {}: {}", fname, e.what());
            return false;
        }
        return true;
    }

    nlohmann::json RelayContact::extract_status() const
    {
        nlohmann::json obj{
            {"lastUpdated", _timestamp.time_since_epoch().count()},
            {"publicRouter", _addr.is_public()},
            {"identity", _router_id.to_string()},
            {"address", _addr.to_string()}};

        return obj;
    }

    std::string RelayContact::to_string() const
    {
        return "RCv{}[{} @ {}, t={}]"_format(VERSION, _router_id, _addr, _timestamp.time_since_epoch().count());
    }

    bool RelayContact::has_ip_overlap(const RelayContact& other, uint8_t netmask) const
    {
        return (_addr.to_ipv4() / netmask).contains(other._addr.to_ipv4());
    }

    bool RelayContact::is_outdated(std::chrono::milliseconds now) const
    {
        return now >= _timestamp.time_since_epoch() + OUTDATED_AGE;
    }

    bool RelayContact::is_expired(std::chrono::milliseconds now) const
    {
        return now >= _timestamp.time_since_epoch() + LIFETIME;
    }

    std::chrono::milliseconds RelayContact::time_to_expiry(std::chrono::milliseconds now) const
    {
        const auto expiry = _timestamp.time_since_epoch() + LIFETIME;
        return now < expiry ? expiry - now : 0s;
    }

    std::chrono::milliseconds RelayContact::age(std::chrono::milliseconds now) const
    {
        auto delta = now - _timestamp.time_since_epoch();
        return delta > 0s ? delta : 0s;
    }

    bool RelayContact::expires_within_delta(std::chrono::milliseconds now, std::chrono::milliseconds dlt) const
    {
        return time_to_expiry(now) <= dlt;
    }

    static const std::unordered_set<std::string_view> obsolete_bootstraps{
        "7a16ac0b85290bcf69b2f3b52456d7e989ac8913b4afbb980614e249a3723218"sv,
        "e6b3a6fe5e32c379b64212c72232d65b0b88ddf9bbaed4997409d329f8519e0b"sv,
    };

    bool RelayContact::is_obsolete() const { return obsolete_bootstraps.contains(_router_id.ToHex()); }

    bool RelayContact::address_changed(const RelayContact& other) const
    {
        return std::tie(_addr, _addr6) != std::tie(other._addr, other._addr6);
    }

    LocalRC::LocalRC(Ed25519SecretKey secret, quic::Address local, NetID netid) : _secret_key{std::move(secret)}
    {
        _router_id.assign(_secret_key.pubkey_span());
        _addr = std::move(local);
        _netid = netid;
        if (_addr.is_ipv6())
            _addr6.emplace(&_addr.in6());
        resign();
    }

    RemoteRC LocalRC::to_remote() const { return RemoteRC{_payload, _netid}; }

    void LocalRC::bt_sign_and_store(oxenc::bt_dict_producer&& btdp)
    {
        btdp.append_signature("~", [this](std::span<const std::byte> to_sign) -> std::span<const std::byte, SIGSIZE> {
            _secret_key.sign(_signature, to_sign);
            return _signature;
        });

        auto v = btdp.view();
        _payload.resize(v.size());
        std::memcpy(_payload.data(), v.data(), v.size());
    }

    oxenc::bt_dict_producer LocalRC::bt_encode_for_signing()
    {
        oxenc::bt_dict_producer btdp;
        btdp.append("", VERSION);

        if (_netid != NetID::MAINNET)
            btdp.append("#", static_cast<int>(_netid));

        std::array<unsigned char, 18> buf;

        {
            if (not _addr.is_ipv4())
                throw std::runtime_error{"Unable to encode RC: addr is not IPv4"};

            auto in4 = _addr.in4();

            std::memcpy(buf.data(), &in4.sin_addr.s_addr, 4);
            std::memcpy(buf.data() + 4, &in4.sin_port, 2);

            btdp.append("4", std::span<const uint8_t>{buf.data(), 6});
        }

        if (_addr6)
        {
            if (not _addr.is_ipv6())
                throw std::runtime_error{"Unable to encode RC: addr6 is set but is not IPv6"};

            auto in6 = _addr.in6();

            std::memcpy(buf.data(), &in6.sin6_addr.s6_addr, 16);
            std::memcpy(buf.data() + 16, &in6.sin6_port, 2);

            btdp.append("6", std::span<const uint8_t>{buf.data(), 18});
        }

        btdp.append("p", _router_id.to_view());

        btdp.append("t", _timestamp.time_since_epoch().count());

        static_assert(llarp::LOKINET_VERSION.size() == 3);
        btdp.append("v", std::span{llarp::LOKINET_VERSION});

        return btdp;
    }

    void LocalRC::resign()
    {
        _timestamp = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
        bt_sign_and_store(bt_encode_for_signing());
    }

    /// Mutators for the private member attributes. Calling on the mutators
    /// will clear the current signature and re-sign the RC
    void LocalRC::set_addr(quic::Address new_addr)
    {
        if (!new_addr.is_ipv4() || !new_addr.is_public())
            throw std::invalid_argument{"set_addr requires a public IPv4 address/port"};
        _addr = std::move(new_addr);
        resign();
    }

    void LocalRC::set_addr6(quic::Address new_addr)
    {
        if (!new_addr.is_ipv6() || !new_addr.is_public())
            throw std::invalid_argument{"set_addr6 requires a public IPv6 address/port"};
        _addr6 = std::move(new_addr);
        resign();
    }
    void LocalRC::clear_addr6() { _addr6.reset(); }

    void LocalRC::set_router_id(RouterID rid)
    {
        _router_id = std::move(rid);
        resign();
    }

    RemoteRC::RemoteRC(std::string_view data, NetID netid, bool accept_expired)
    {
        if (data.size() > MAX_RC_SIZE)
            throw std::invalid_argument{"Invalid RC: exceeds maximum size"};
        _payload = data;
        load(netid, accept_expired);
    }

    template <>
    RemoteRC::RemoteRC(const fs::path& fname, NetID netid, bool accept_expired)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        _payload = util::file_to_string(fname);
        load(netid, accept_expired);
    }

}  // namespace llarp
