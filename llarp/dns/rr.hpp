#pragma once

#include "name.hpp"
#include <llarp/net/net_int.hpp>
#include <llarp/util/status.hpp>
#include <llarp/util/underlying.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace llarp::dns
{

  /// resource record types enum.
  enum class RRType : uint16_t
  {
    SRV = 33,
    AAAA = 28,
    TXT = 16,
    MX = 15,
    PTR = 12,
    CNAME = 5,
    NS = 2,
    A = 1
  };

  std::optional<RRType>
  get_rr_type(uint16_t i);

  std::string
  ToString(RRType t);

  class RData
  {
    bstring_t _raw;

   public:
    RData() = default;
    RData(const RData&) = default;
    RData(RData&&) = default;

    RData&
    operator=(const RData&) = default;
    RData&
    operator=(RData&&) = default;

    /// construct from view, modifies view to contain remaining unread data.
    explicit RData(byte_view_t& view);

    /// construct rdata holding dns labels.
    explicit RData(std::vector<std::string_view> dns_labels);
    /// construct holding ipv4 address
    explicit RData(net::ipv4addr_t ip);
    /// construct holding ipv6 address
    explicit RData(net::ipv6addr_t ip);

    /// mx record constructor.
    explicit RData(uint16_t priority, std::vector<std::string_view> dns_labels);

    /// construct from raw buffer.
    explicit RData(bstring_t&& raw);

    [[nodiscard]] constexpr const auto&
    data() const
    {
      return _raw;
    }

    std::vector<std::string_view>
    view_dns_labels() const;

    inline std::string
    as_dns_name() const
    {
      std::string name;

      for (auto part : view_dns_labels())
        name += fmt::format("{}.", part);

      return name.substr(0, name.size() - 2);
    }

    util::StatusObject
    to_json() const;

    std::string
    ToString() const;
  };

  class ResourceRecord
  {
    explicit ResourceRecord(bstring_t&&);

   public:
    ResourceRecord() = default;

    ResourceRecord(std::string name, RRType type, RData rdata, uint32_t ttl);

    /// construct from raw bytes, modifies raw.
    explicit ResourceRecord(byte_view_t& raw);

    bstring_t
    encode_dns() const;

    util::StatusObject
    to_json() const;

    std::string
    ToString() const;

    bool
    HasCNameForTLD(const std::string& tld) const;

    std::vector<std::string> rr_name_labels;
    RRType rr_type;
    uint32_t rr_ttl;
    RData rr_data;
  };

  [[deprecated]] constexpr auto qTypePTR = to_underlying(RRType::PTR);
  [[deprecated]] constexpr auto qTypeA = to_underlying(RRType::A);
  [[deprecated]] constexpr auto qTypeAAAA = to_underlying(RRType::AAAA);
  [[deprecated]] constexpr auto qTypeSRV = to_underlying(RRType::SRV);
  [[deprecated]] constexpr auto qTypeCNAME = to_underlying(RRType::CNAME);
  [[deprecated]] constexpr auto qTypeMX = to_underlying(RRType::MX);
  [[deprecated]] constexpr auto qTypeNS = to_underlying(RRType::NS);
}  // namespace llarp::dns

template <>
constexpr inline bool llarp::IsToStringFormattable<llarp::dns::ResourceRecord> = true;

template <>
constexpr inline bool llarp::IsToStringFormattable<llarp::dns::RData> = true;

template <>
constexpr inline bool llarp::IsToStringFormattable<llarp::dns::RRType> = true;
