#include "constants/version.hpp"
#include "crypto/crypto.hpp"
#include "net/net.hpp"
#include "router_contact.hpp"
#include "util/bencode.hpp"
#include "util/buffer.hpp"
#include "util/file.hpp"
#include "util/time.hpp"

#include <oxenc/bt_serialize.h>

namespace llarp
{
  LocalRC
  LocalRC::make(const SecretKey secret, oxen::quic::Address local)
  {
    return *new LocalRC{secret, local};
  }

  LocalRC::LocalRC(const SecretKey secret, oxen::quic::Address local)
      : _secret_key{std::move(secret)}
  {
    _router_id = llarp::seckey_to_pubkey(_secret_key);
    _addr = std::move(local);
    if (_addr.is_ipv6())
      _addr6.emplace(&_addr.in6());
    resign();
  }

  LocalRC::LocalRC(std::string payload, const SecretKey sk) : _secret_key{std::move(sk)}
  {
    _router_id = llarp::seckey_to_pubkey(_secret_key);

    try
    {
      oxenc::bt_dict_consumer btdc{payload};
      bt_load(btdc);

      btdc.require_signature("~", [this](ustring_view msg, ustring_view sig) {
        if (sig.size() != 64)
          throw std::runtime_error{"Invalid signature: not 64 bytes"};

        if (is_expired(time_now_ms()))
          throw std::runtime_error{"Unable to verify expired RemoteRC!"};

        // TODO: revisit if this is needed; detail from previous implementation
        const auto* net = net::Platform::Default_ptr();

        if (net->IsBogon(addr().in4()) and BLOCK_BOGONS)
        {
          auto err = "Unable to verify expired RemoteRC!";
          log::info(logcat, err);
          throw std::runtime_error{err};
        }

        if (not crypto::verify(router_id(), msg, sig))
          throw std::runtime_error{"Failed to verify RemoteRC"};
      });
    }
    catch (const std::exception& e)
    {
      log::warning(logcat, "Failed to parse LocalRC: {}", e.what());
      throw;
    }
  }

  void
  LocalRC::bt_sign(oxenc::bt_dict_producer& btdp)
  {
    _signature.clear();

    btdp.append_signature("~", [this](ustring_view to_sign) {
      std::array<unsigned char, 64> sig;

      if (!crypto::sign(const_cast<unsigned char*>(sig.data()), _secret_key, to_sign))
        throw std::runtime_error{"Failed to sign RC"};

      _signature = {sig.data(), sig.size()};
      return sig;
    });

    _payload = btdp.view<unsigned char>();
  }

  void
  LocalRC::bt_encode(oxenc::bt_dict_producer& btdp)
  {
    btdp.append("", RC_VERSION);

    std::array<unsigned char, 18> buf;

    {
      if (not _addr.is_ipv4())
        throw std::runtime_error{"Unable to encode RC: addr is not IPv4"};

      auto in4 = _addr.in4();

      std::memcpy(buf.data(), &in4.sin_addr.s_addr, 4);
      std::memcpy(buf.data() + 4, &in4.sin_port, 2);

      btdp.append("4", ustring_view{buf.data(), 6});
    }

    if (_addr6)
    {
      if (not _addr.is_ipv6())
        throw std::runtime_error{"Unable to encode RC: addr6 is set but is not IPv6"};

      auto in6 = _addr.in6();

      std::memcpy(buf.data(), &in6.sin6_addr.s6_addr, 16);
      std::memcpy(buf.data() + 16, &in6.sin6_port, 2);

      btdp.append("6", ustring_view{buf.data(), 18});
    }

    if (ACTIVE_NETID != llarp::LOKINET_DEFAULT_NETID)
      btdp.append("i", ACTIVE_NETID);

    btdp.append("p", _router_id.ToView());

    btdp.append("t", _timestamp.time_since_epoch().count());

    static_assert(llarp::LOKINET_VERSION.size() == 3);
    btdp.append(
        "v", std::string_view{reinterpret_cast<const char*>(llarp::LOKINET_VERSION.data()), 3});
  }

  void
  LocalRC::resign()
  {
    set_systime_timestamp();
    oxenc::bt_dict_producer btdp;
    bt_encode(btdp);
    bt_sign(btdp);
  }
}  // namespace llarp
