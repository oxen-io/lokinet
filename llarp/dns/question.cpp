#include "question.hpp"
#include "bits.hpp"
#include "llarp/dns/name.hpp"
#include <llarp/util/underlying.hpp>

#include <oxenc/endian.h>
#include <llarp/util/logging.hpp>
#include <llarp/util/str.hpp>
#include <stdexcept>
#include <string_view>

#include <oxen/log/format.hpp>
#include <type_traits>
#include <vector>

namespace llarp::dns
{

  static auto logcat = log::Cat("dns");

  Question::Question(std::vector<std::string_view> labels, RRType type)
      : qtype{to_underlying(type)}, qclass{bits::qclass_in}
  {
    if (labels.empty())
      throw std::invalid_argument{"dns labels are empty"};
    bool prev_was_empty{labels.front().empty()};
    for (const auto& label : labels)
    {
      bool _empty = label.empty();

      if (_empty and prev_was_empty)
        throw std::invalid_argument{"more than one empty label given in a row"};

      prev_was_empty = _empty;

      if (label.size() > max_dns_label_size)
        throw std::invalid_argument{
            fmt::format("dns label too big: {} > {}", label.size(), max_dns_label_size)};
      _qname_labels.emplace_back(label);
    }
    if (_qname_labels.back().empty())
      _qname_labels.pop_back();
  }

  bstring_t
  Question::encode_dns() const
  {
    bstring_t ret(size_t{4}, uint8_t{});
    oxenc::write_host_as_big<uint16_t>(qtype, &ret[0]);
    oxenc::write_host_as_big<uint16_t>(bits::qclass_in, &ret[2]);
    return encode_dns_labels(_qname_labels) + ret;
  }

  Question::Question(byte_view_t& str)
  {
    _qname_labels = decode_dns_labels(str);

    if (str.size() < 4)
      throw std::invalid_argument{"data too small"};

    qtype = oxenc::load_big_to_host<uint16_t>(&str[0]);
    qclass = oxenc::load_big_to_host<uint16_t>(&str[2]);

    str = str.substr(4);
  }

  bool
  Question::valid() const
  {
    return qclass == bits::qclass_in;
  }

  util::StatusObject
  Question::to_json() const
  {
    return util::StatusObject{{"qname", qname()}, {"qtype", qtype}, {"qclass", qclass}};
  }

  bool
  Question::IsName(const std::string& other) const
  {
    auto name = qname();
    return other == name or (other + ".") == name;
  }

  bool
  Question::IsLocalhost() const
  {
    auto sz = _qname_labels.size();

    return sz >= 2 and is_lokinet_tld(_qname_labels.back())
        and _qname_labels[sz - 2] == "localhost";
  }

  bool
  Question::HasSubdomains() const
  {
    return _qname_labels.size() > 2;
  }

  std::string
  Question::Subdomains() const
  {
    if (not HasSubdomains())
      return "";

    auto it = _qname_labels.rbegin();
    ++it;
    ++it;

    std::string ret{*it};
    ++it;

    for (auto itr = it; itr != _qname_labels.rend(); ++itr)
    {
      ret = fmt::format("{}.{}", *itr, ret);
    }

    return ret;
  }

  std::string
  Question::qname() const
  {
    return fmt::format("{}", fmt::join(_qname_labels, "."));
  }

  std::string_view
  Question::tld() const
  {
    if (_qname_labels.size() < 2)
      return "";
    return _qname_labels[_qname_labels.size() - 1];
  }

  std::string
  Question::Domain() const
  {
    auto itr = _qname_labels.rbegin();
    if (_qname_labels.size() > 1)
      ++itr;
    return *itr;
  }

  std::string
  Question::ToString() const
  {
    return fmt::format("[DNSQuestion qname={} qtype={:x} qclass={:x}]", qname(), qtype, qclass);
  }

  std::vector<std::string_view>
  Question::view_dns_labels() const
  {
    std::vector<std::string_view> view;
    for (const auto& label : _qname_labels)
      view.emplace_back(label);
    return view;
  }
}  // namespace llarp::dns
