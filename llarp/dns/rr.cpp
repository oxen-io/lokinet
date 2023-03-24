#include "rr.hpp"
#include "bits.hpp"
#include "llarp/dns/name.hpp"
#include "llarp/dns/question.hpp"
#include "llarp/util/time.hpp"

#include <cstddef>
#include <cstdint>
#include <llarp/util/formattable.hpp>
#include <llarp/util/mem.hpp>
#include <llarp/util/str.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/underlying.hpp>

#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <vector>
#include <algorithm>

#include <oxenc/endian.h>
#include <oxenc/hex.h>

namespace llarp::dns
{
  static auto logcat = log::Cat("dns");

  namespace
  {
    template <typename T>
    [[nodiscard]] auto
    encode_rdata(const T& rdata)
    {
      const std::size_t sz = rdata.size();
      if (sz > 65536)
        throw std::invalid_argument{"rdata too big: {} > {}"_format(sz, 65536)};
      uint16_t len{static_cast<uint16_t>(sz)};
      bstring_t vec;
      vec.resize(sz + 2);
      auto* ptr = vec.data();
      oxenc::write_host_as_big(len, ptr);
      ptr += 2;
      std::copy(rdata.begin(), rdata.end(), ptr);
      return vec;
    }

    [[nodiscard]] byte_view_t
    decode_rdata(byte_view_t& raw)
    {
      if (raw.size() < 2)
        throw std::invalid_argument{
            ("resource record data too small to be a resource record: {} < 2"_format(raw.size()))};
      size_t len = oxenc::load_big_to_host<uint16_t>(raw.data());
      raw = raw.substr(2);
      if (raw.size() < len)
        throw std::invalid_argument{
            "resource record data buffer too small: {} < {}"_format(raw.size(), len)};
      byte_view_t ret{raw.data(), len};
      raw = raw.substr(len);
      return ret;
    }

    [[nodiscard]] byte_view_t
    decode_rdata(bstring_t&& raw)
    {
      byte_view_t view{raw};
      return decode_rdata(view);
    }
  }  // namespace

  ResourceRecord::ResourceRecord(std::string name, RRType type, RData data, uint32_t ttl)
      : rr_name_labels{}, rr_type{type}, rr_ttl{ttl}, rr_data{std::move(data)}
  {
    auto parts = split(name, ".");
    rr_name_labels.insert(rr_name_labels.cbegin(), parts.begin(), parts.end());
  }

  bstring_t
  ResourceRecord::encode_dns() const
  {
    bstring_t ret;
    ret.resize(8);

    auto ptr = ret.data();
    oxenc::write_host_as_big(static_cast<std::underlying_type_t<decltype(rr_type)>>(rr_type), ptr);
    ptr += 2;
    oxenc::write_host_as_big(bits::qclass_in, ptr);
    ptr += 2;
    oxenc::write_host_as_big(rr_ttl, ptr);
    ptr += 4;

    ret += rr_data.data();
    return ret;
  }

  std::optional<RRType>
  get_rr_type(uint16_t i)
  {
    const std::unordered_set<std::underlying_type_t<RRType>> mapping = {
        to_underlying(RRType::SRV),
        to_underlying(RRType::A),
        to_underlying(RRType::AAAA),
        to_underlying(RRType::CNAME),
        to_underlying(RRType::MX),
        to_underlying(RRType::NS),
        to_underlying(RRType::PTR),
        to_underlying(RRType::TXT)};
    if (mapping.count(i))
      return RRType{i};
    return std::nullopt;
  }

  std::string
  ToString(RRType t)
  {
    std::string ret;
    switch (t)
    {
      case RRType::SRV:
        ret = "SRV";
        break;
      case RRType::A:
        ret = "A";
        break;
      case RRType::AAAA:
        ret = "AAAA";
        break;
      case RRType::CNAME:
        ret = "CNAME";
        break;
      case RRType::MX:
        ret = "MX";
        break;
      case RRType::NS:
        ret = "NS";
        break;
      case RRType::PTR:
        ret = "PTR";
        break;
      case RRType::TXT:
        ret = "TXT";
        break;
    }
    return ret;
  }

  ResourceRecord::ResourceRecord(bstring_t&& raw)
  {
    if (raw.size() < 12)
      throw std::invalid_argument{"resource record too small: {} < {}"_format(raw.size(), 12)};
    auto t = oxenc::load_big_to_host<uint16_t>(raw.data());
    auto maybe_type = get_rr_type(t);
    if (not maybe_type)
      throw std::invalid_argument{"unknown RR type: {:x}"_format(t)};

    rr_type = *maybe_type;

    raw = raw.substr(2);
    if (auto qc = oxenc::load_big_to_host<uint16_t>(raw.data()); qc != bits::qclass_in)
      throw std::invalid_argument{
          "resource record does not have qclass IN, has {:x} instead"_format(qc)};

    raw = raw.substr(2);
    rr_ttl = oxenc::load_big_to_host<uint32_t>(raw.data());
    raw = raw.substr(4);

    rr_data = RData{std::move(raw)};
  }

  util::StatusObject
  ResourceRecord::to_json() const
  {
    return util::StatusObject{
        {"name", "{{}}"_format(fmt::join(rr_name_labels, "."))},
        {"type", rr_type},
        {"ttl", rr_ttl},
        {"rr_data", rr_data.to_json()}};
  }

  std::string
  ResourceRecord::ToString() const
  {
    return fmt::format(
        "[RR name={{}} type={} ttl={} rr_data={}]",
        fmt::join(rr_name_labels, "."),
        rr_type,
        rr_ttl,
        rr_data);
  }

  bool
  ResourceRecord::HasCNameForTLD(const std::string& tld) const
  {
    if (rr_type != RRType::CNAME)
      return false;
    if (auto labels = rr_data.view_dns_labels(); labels.size() > 1)
      return labels[labels.size() - 1] == tld;
    return false;
  }

  RData::RData(std::vector<std::string_view> dns_labels)
      : _raw{encode_rdata(encode_dns_labels(dns_labels))}
  {}

  RData::RData(net::ipv4addr_t ip) : _raw{}
  {
    _raw.resize(4);
    std::memcpy(_raw.data(), &ip.n, 4);
  }

  RData::RData(net::ipv6addr_t ip) : _raw{}
  {
    _raw.resize(16);
    std::memcpy(_raw.data(), &ip.n, 16);
  }

  RData::RData(bstring_t&& raw) : _raw{raw}
  {}

  RData::RData(uint16_t priority, std::vector<std::string_view> dns_labels)
  {
    _raw.resize(2);
    oxenc::write_host_as_big(priority, _raw.data());
    _raw += encode_dns_labels(dns_labels);
  }

  nlohmann::json
  RData::to_json() const
  {
    return {{"rdata_hex", oxenc::to_hex(_raw.begin(), _raw.end())}};
  }

  std::vector<std::string_view>
  RData::view_dns_labels() const
  {
    byte_view_t view{_raw};
    return decode_dns_label_views(view);
  }

  std::string
  RData::ToString() const
  {
    return fmt::format("[RData hex={}]", oxenc::to_hex(_raw.begin(), _raw.end()));
  }

  ResourceRecord::ResourceRecord(byte_view_t& raw) : ResourceRecord{bstring_t{decode_rdata(raw)}}
  {}
}  // namespace llarp::dns
