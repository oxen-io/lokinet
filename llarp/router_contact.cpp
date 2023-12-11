#include "router_contact.hpp"

#include "constants/version.hpp"
#include "crypto/crypto.hpp"
#include "net/net.hpp"
#include "util/bencode.hpp"
#include "util/buffer.hpp"
#include "util/file.hpp"

#include <oxenc/bt_serialize.h>

namespace llarp
{
  void
  RouterContact::bt_load(oxenc::bt_dict_consumer& data)
  {
    if (int rc_ver = data.require<uint8_t>(""); rc_ver != RC_VERSION)
      throw std::runtime_error{"Invalid RC: do not know how to parse v{} RCs"_format(rc_ver)};

    auto ipv4_port = data.require<std::string_view>("4");

    if (ipv4_port.size() != 6)
      throw std::runtime_error{
          "Invalid RC address: expected 6-byte IPv4 IP/port, got {}"_format(ipv4_port.size())};

    sockaddr_in s4;
    s4.sin_family = AF_INET;

    std::memcpy(&s4.sin_addr.s_addr, ipv4_port.data(), 4);
    std::memcpy(&s4.sin_port, ipv4_port.data() + 4, 2);

    _addr = oxen::quic::Address{&s4};

    if (!_addr.is_public())
      throw std::runtime_error{"Invalid RC: IPv4 address is not a publicly routable IP"};

    if (auto ipv6_port = data.maybe<std::string_view>("6"))
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

    auto netid = data.maybe<std::string_view>("i").value_or(llarp::LOKINET_DEFAULT_NETID);
    if (netid != ACTIVE_NETID)
      throw std::runtime_error{
          "Invalid RC netid: expected {}, got {}; this is an RC for a different network!"_format(
              ACTIVE_NETID, netid)};

    _router_id.from_string(data.require<std::string_view>("p"));

    // auto pk = data.require<std::string_view>("p");

    // if (pk.size() != RouterID::SIZE)
    //   throw std::runtime_error{"Invalid RC: router id has invalid size {}"_format(pk.size())};

    // std::memcpy(_router_id.data(), pk.data(), RouterID::SIZE);

    _timestamp = rc_time{std::chrono::seconds{data.require<int64_t>("t")}};

    auto ver = data.require<ustring_view>("v");

    if (ver.size() != 3)
      throw std::runtime_error{
          "Invalid RC router version: received {} bytes (!= 3)"_format(ver.size())};

    for (int i = 0; i < 3; i++)
      _router_version[i] = ver[i];
  }

  bool
  RouterContact::write(const fs::path& fname) const
  {
    auto bte = view();

    try
    {
      util::buffer_to_file(fname, bte.data(), bte.size());
    }
    catch (const std::exception& e)
    {
      log::error(logcat, "Failed to write RC to {}: {}", fname, e.what());
      return false;
    }
    return true;
  }

  util::StatusObject
  RouterContact::extract_status() const
  {
    util::StatusObject obj{
        {"lastUpdated", _timestamp.time_since_epoch().count()},
        {"publicRouter", is_public_addressable()},
        {"identity", _router_id.ToString()},
        {"address", _addr.to_string()}};

    // if (routerVersion)
    // {
    //   obj["routerVersion"] = routerVersion->ToString();
    // }
    // std::vector<util::StatusObject> srv;
    // for (const auto& record : srvRecords)
    // {
    //   srv.emplace_back(record.ExtractStatus());
    // }
    // obj["srvRecords"] = srv;

    return obj;
  }

  bool
  RouterContact::BDecode(llarp_buffer_t* buf)
  {
    // TODO: unfuck all of this

    (void)buf;

    // clear();

    // if (*buf->cur == 'd')  // old format
    // {
    //   return DecodeVersion_0(buf);
    // }
    // else if (*buf->cur != 'l')  // if not dict, should be new format and start with list
    // {
    //   return false;
    // }

    // try
    // {
    //   std::string_view buf_view(reinterpret_cast<char*>(buf->cur), buf->size_left());
    //   oxenc::bt_list_consumer btlist(buf_view);

    //   uint64_t outer_version = btlist.consume_integer<uint64_t>();

    //   if (outer_version == 1)
    //   {
    //     bool decode_result = DecodeVersion_1(btlist);

    //     // advance the llarp_buffer_t since lokimq serialization is unaware of it.
    //     // FIXME: this is broken (current_buffer got dropped), but the whole thing is getting
    //     // replaced.
    //     // buf->cur += btlist.
    //     //    current_buffer().data() - buf_view.data() + 1;

    //     return decode_result;
    //   }
    //   else
    //   {
    //     log::warning(logcat, "Received RouterContact with unkown version ({})", outer_version);
    //     return false;
    //   }
    // }
    // catch (const std::exception& e)
    // {
    //   log::debug(logcat, "RouterContact::BDecode failed: {}", e.what());
    // }

    return false;
  }

  bool
  RouterContact::decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    (void)key;

    // TOFIX: fuck everything about llarp_buffer_t

    // if (!BEncodeMaybeReadDictEntry("a", addr, read, key, buf))
    //   return false;

    // if (!BEncodeMaybeReadDictEntry("i", netID, read, key, buf))
    //   return false;

    // if (!BEncodeMaybeReadDictEntry("k", _router_id, read, key, buf))
    //   return false;

    // if (key.startswith("r"))
    // {
    //   RouterVersion r;
    //   if (not r.BDecode(buf))
    //     return false;
    //   routerVersion = r;
    //   return true;
    // }

    // if (not BEncodeMaybeReadDictList("s", srvRecords, read, key, buf))
    //   return false;

    // if (!BEncodeMaybeReadDictEntry("p", enckey, read, key, buf))
    //   return false;

    // if (!BEncodeMaybeReadDictInt("u", _timestamp, read, key, buf))
    //   return false;

    // if (!BEncodeMaybeReadDictInt("v", version, read, key, buf))
    //   return false;

    // if (key.startswith("x") and serializeExit)
    // {
    //   return bencode_discard(buf);
    // }

    // if (!BEncodeMaybeReadDictEntry("z", signature, read, key, buf))
    //   return false;

    return read or bencode_discard(buf);
  }

  bool
  RouterContact::is_public_addressable() const
  {
    if (_router_version.empty())
      return false;

    return _addr.is_addressable();
  }

  bool
  RouterContact::is_expired(llarp_time_t now) const
  {
    return age(now) >= _timestamp.time_since_epoch() + LIFETIME;
  }

  llarp_time_t
  RouterContact::time_to_expiry(llarp_time_t now) const
  {
    const auto expiry = _timestamp.time_since_epoch() + LIFETIME;
    return now < expiry ? expiry - now : 0s;
  }

  llarp_time_t
  RouterContact::age(llarp_time_t now) const
  {
    auto delta = now - _timestamp.time_since_epoch();
    return delta > 0s ? delta : 0s;
  }

  bool
  RouterContact::expires_within_delta(llarp_time_t now, llarp_time_t dlt) const
  {
    return time_to_expiry(now) <= dlt;
  }

  static constexpr std::array obsolete_bootstraps = {
      "7a16ac0b85290bcf69b2f3b52456d7e989ac8913b4afbb980614e249a3723218"sv,
      "e6b3a6fe5e32c379b64212c72232d65b0b88ddf9bbaed4997409d329f8519e0b"sv,
  };

  bool
  RouterContact::is_obsolete_bootstrap() const
  {
    for (const auto& k : obsolete_bootstraps)
    {
      if (_router_id.ToHex() == k)
        return true;
    }
    return false;
  }
}  // namespace llarp
