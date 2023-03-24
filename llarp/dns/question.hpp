#pragma once

#include "name.hpp"
#include "rr.hpp"

#include <cstdint>
#include <llarp/net/net_int.hpp>
#include <string_view>
#include <vector>

#include <llarp/util/status.hpp>
#include <llarp/util/str.hpp>

namespace llarp::dns
{

  struct Question
  {
    Question() = default;
    Question(const Question&) = default;
    Question(Question&&) = default;

    Question&
    operator=(const Question&) = default;
    Question&
    operator=(Question&&) = default;

    explicit Question(byte_view_t& raw);

    Question(std::string name, RRType type) : Question{split_dns_name(name), type}
    {}

    Question(std::vector<std::string_view> labels, RRType type);

    /// encode to dns wire proto.
    bstring_t
    encode_dns() const;

    std::string
    ToString() const;

    uint16_t qtype;
    uint16_t qclass;

    /// construct the full domain name including trailing dot.
    std::string
    qname() const;

    /// determine if we match a name
    bool
    IsName(const std::string& other) const;

    /// is the name [something.]localhost.loki. ?
    bool
    IsLocalhost() const;

    /// return true if we have subdomains in ths question
    bool
    HasSubdomains() const;

    /// get subdomain(s), if any, from qname
    std::string
    Subdomains() const;

    /// get the dns label before the gtld, including the gtld and no trailing full stop.
    std::string
    Domain() const;

    /// get the tld for the question's qname.
    std::string_view
    tld() const;

    inline bool
    HasTLD(std::string_view _tld) const [[deprecated]]
    {
      return tld() == _tld;
    }

    util::StatusObject
    to_json() const;

    /// return true of this question has sane data.
    bool
    valid() const;

    /// get a view over all the dns labels for this question.
    std::vector<std::string_view>
    view_dns_labels() const;

   private:
    std::vector<std::string> _qname_labels;
  };
}  // namespace llarp::dns

namespace llarp
{
  template <>
  constexpr inline bool IsToStringFormattable<dns::Question> = true;
}
