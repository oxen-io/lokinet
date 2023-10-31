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
  LocalRC::LocalRC(std::string payload, const SecretKey sk) : 
      _secret_key{std::move(sk)}
  {
    _router_id = llarp::seckey_to_pubkey(_secret_key);

    try
    {
      oxenc::bt_dict_consumer btdc{payload};
      bt_load(btdc);
      bt_sign(btdc);
    }
    catch (const std::exception& e)
    {
      log::warning(logcat, "Failed to parse LocalRC: {}", e.what());
      throw;
    }
  }

  void
  LocalRC::bt_sign(oxenc::bt_dict_consumer& btdc)
  {
    _signature.clear();

    btdc.require_signature("~", [&](ustring_view msg, ustring_view s) {
      if (!crypto::sign(const_cast<unsigned char*>(s.data()), _secret_key, msg))
        throw std::runtime_error{"Failed to sign RC"};

      _signature = s;
      _signed_payload = msg;
    });
  }

  void
  LocalRC::bt_encode(oxenc::bt_dict_producer& btdp) const
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

    btdp.append_signature("~", [&](ustring_view to_sign) {
      std::array<unsigned char, 64> sig;

      if (!crypto::sign(sig.data(), _secret_key, to_sign))
        throw std::runtime_error{"Failed to sign LocalRC"};
      
      return sig;
    });
  }

  void
  LocalRC::resign()
  {
    oxenc::bt_dict_consumer btdc{_signed_payload};
    bt_sign(btdc);

    // DISCUSS: should we also update the timestamp when we re-sign?
    //  -> Is the timestamp referring to signing time or time the RC was originally created?
  }
}  // namespace llarp
