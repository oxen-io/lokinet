#pragma once

#include <algorithm>
#include <llarp/net/net_int.hpp>
#include <llarp/util/str.hpp>

#include <string>
#include <optional>
#include <string_view>
#include <vector>
#include <array>

namespace llarp::dns
{

  /// the max size of one dns label in bytes.
  constexpr auto max_dns_label_size = 63;

  /// return true if this tld is a lokinet tld.
  inline bool
  is_lokinet_tld(std::string_view str)
  {
    constexpr std::array lokinet_tlds = {"loki"sv, "snode"sv};
    for (const auto& tld : lokinet_tlds)
    {
      if (string_iequal(tld, str))
        return true;
    }
    return false;
  }

  std::optional<huint128_t>
  DecodePTR(std::string_view name);

  bool
  NameIsReserved(std::string_view name);

  /// encode a series of dns labels into dns wire protocol.
  bstring_t
  encode_dns_labels(std::vector<std::string_view> labels);

  /// encode a series of dns labels into dns wire protocol.
  bstring_t
  encode_dns_labels(std::vector<std::string> labels);

  /// encode a dns name in dotted form and encode it to dns wire protocol.
  bstring_t
  encode_dns_name(std::string_view name);

  /// decode dns lables from dns wire protocol
  std::vector<std::string>
  decode_dns_labels(byte_view_t& buf);

  /// decode dns lables from dns wire protocol, no copy version.
  std::vector<std::string_view>
  decode_dns_label_views(byte_view_t& buf);

  /// named helper functiopn to split a dns name into labels.
  template <typename T>
  auto
  split_dns_name(const T& name)
  {
    return split(name, ".");
  }

}  // namespace llarp::dns
