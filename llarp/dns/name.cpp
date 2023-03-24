#include "name.hpp"
#include <cstdint>
#include <limits>
#include <llarp/net/net.hpp>
#include <llarp/net/ip.hpp>
#include <llarp/util/str.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "llarp/util/types.hpp"
#include <fmt/core.h>
#include <oxenc/hex.h>

namespace llarp::dns
{
  namespace
  {
    template <typename Label_t>
    std::vector<Label_t>
    decode(byte_view_t& bstr)
    {
      std::vector<Label_t> labels;
      size_t label_len{};
      do
      {
        if (bstr.empty())
          throw std::invalid_argument{"unexpected end of data"};
        label_len = bstr[0];
        auto label_size = label_len + 1;
        if (bstr.size() < label_size)
          throw std::invalid_argument{
              "dns label size mismatch: {} < {}"_format(bstr.size(), label_size)};

        if (label_len)
          labels.emplace_back(
              reinterpret_cast<const typename Label_t::value_type*>(&bstr[1]), label_len);
        bstr = bstr.substr(label_size);
      } while (label_len > 0);

      // remove trailing empty label.
      if (labels.back().empty())
        labels.pop_back();

      return labels;
    }

    template <typename Ret_t, typename Val_t>
    Ret_t
    encode(std::vector<Val_t> labels)
    {
      using char_type_t = typename Ret_t::value_type;
      Ret_t str;
      for (const auto& label : labels)
      {
        if (label.empty())
          break;
        auto label_size = label.size();
        if (label_size > max_dns_label_size)
          throw std::invalid_argument{
              "dns label too big: {} > {}"_format(label_size, max_dns_label_size)};
        str += static_cast<char_type_t>(label_size);
        str += std::basic_string_view<char_type_t>{
            reinterpret_cast<const char_type_t*>(label.data()), label_size};
      }
      str += char_type_t{};
      return str;
    }
  }  // namespace

  std::vector<std::string>
  decode_dns_labels(byte_view_t& bstr)
  {
    return decode<std::string>(bstr);
  }

  std::vector<std::string_view>
  decode_dns_label_views(byte_view_t& bstr)
  {
    return decode<std::string_view>(bstr);
  }

  bstring_t
  encode_dns_labels(std::vector<std::string_view> labels)
  {
    return encode<bstring_t>(labels);
  }

  bstring_t
  encode_dns_labels(std::vector<std::string> labels)
  {
    return encode<bstring_t>(labels);
  }

  bstring_t
  encode_dns_name(std::string_view name)
  {
    return encode_dns_labels(split_dns_name(name));
  }

  std::optional<huint128_t>
  DecodePTR(std::string_view name)
  {
    bool isV6 = false;
    auto pos = name.find(".in-addr.arpa");
    if (pos == std::string::npos)
    {
      pos = name.find(".ip6.arpa");
      isV6 = true;
    }
    if (pos == std::string::npos)
      return std::nullopt;
    name = name.substr(0, pos + 1);
    const auto numdots = std::count(name.begin(), name.end(), '.');
    if (numdots == 4 && !isV6)
    {
      std::array<uint8_t, 4> q;
      for (int i = 3; i >= 0; i--)
      {
        pos = name.find('.');
        if (!llarp::parse_int(name.substr(0, pos), q[i]))
          return std::nullopt;
        name.remove_prefix(pos + 1);
      }
      return net::ExpandV4(llarp::ipaddr_ipv4_bits(q[0], q[1], q[2], q[3]));
    }
    if (numdots == 32 && name.size() == 64 && isV6)
    {
      // We're going to convert from nybbles a.b.c.d.e.f.0.1.2.3.[...] into hex string
      // "badcfe1032...", then decode the hex string to bytes.
      std::array<char, 32> in;
      auto in_pos = in.data();
      for (size_t i = 0; i < 64; i += 4)
      {
        if (not(oxenc::is_hex_digit(name[i]) and name[i + 1] == '.'
                and oxenc::is_hex_digit(name[i + 2]) and name[i + 3] == '.'))
          return std::nullopt;

        // Flip the nybbles because the smallest one is first
        *in_pos++ = name[i + 2];
        *in_pos++ = name[i];
      }
      assert(in_pos == in.data() + in.size());
      huint128_t ip;
      static_assert(in.size() == 2 * sizeof(ip.h));
      // our string right now is the little endian representation, so load it as such on little
      // endian, or in reverse on big endian.
      if constexpr (oxenc::little_endian)
        oxenc::from_hex(in.begin(), in.end(), reinterpret_cast<uint8_t*>(&ip.h));
      else
        oxenc::from_hex(in.rbegin(), in.rend(), reinterpret_cast<uint8_t*>(&ip.h));

      return ip;
    }
    return std::nullopt;
  }

  bool
  NameIsReserved(std::string_view name)
  {
    constexpr std::array reserved_names = {
        ".snode.snode"sv, ".snode.loki"sv, ".loki.snode"sv, ".loki.loki"sv};

    for (const auto& reserved : reserved_names)
    {
      if (ends_with(name, reserved))  // subdomain foo.loki.loki
        return true;
      if (name == reserved.substr(1))  // loki.loki itself
        return true;
    }
    return false;
  }
}  // namespace llarp::dns
