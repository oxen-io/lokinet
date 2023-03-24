#include "message.hpp"
#include "bits.hpp"
#include "llarp/util/str.hpp"
#include "srv_data.hpp"

#include <cstdint>
#include <future>
#include <llarp/util/buffer.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/net/ip.hpp>

#include <fmt/core.h>
#include <oxenc/endian.h>

#include <array>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace llarp::dns
{

  static auto logcat = log::Cat("dns");

  MessageHeader::MessageHeader(byte_view_t& raw)
  {
    if (raw.size() < MessageHeader::Size)
      throw std::invalid_argument{
          "dns message buffer too small: {} < {}"_format(raw.size(), MessageHeader::Size)};
    for (auto* i : {&id, &fields, &qd_count, &an_count, &ns_count, &ar_count})
    {
      *i = oxenc::load_big_to_host<uint16_t>(raw.data());
      raw = raw.substr(2);
    }
  }

  util::StatusObject
  MessageHeader::to_json() const
  {
    return util::StatusObject{
        {"id", id},
        {"fields", fields},
        {"qd", qd_count},
        {"an", an_count},
        {"ns", ns_count},
        {"ar", ar_count}};
  }

  MessageHeader&
  MessageHeader::rcode(uint16_t code)
  {
    id = bits::make_rcode(code);
    return *this;
  }

  bstring_t
  MessageHeader::encode_dns() const
  {
    bstring_t raw;
    raw.resize(MessageHeader::Size);
    auto ptr = reinterpret_cast<uint16_t*>(raw.data());
    for (uint16_t i : {id, fields, qd_count, an_count, ns_count, ar_count})
    {
      oxenc::write_host_as_big(i, ptr);
      ptr++;
    }
    return raw;
  }

  namespace
  {
    /// decode N dns data types and shrinks raw as it reads.
    /// returns a vector of size N or throws.
    template <typename T>
    [[nodiscard]] auto
    decode_dns(byte_view_t& raw, size_t n)
    {
      std::vector<T> ents;
      while (n > 0)
      {
        ents.emplace_back(raw);
        --n;
      }
      return ents;
    }
  }  // namespace

  Message::Message(byte_view_t& raw) : hdr{raw}
  {
    questions = decode_dns<Question>(raw, hdr.qd_count);
    answers = decode_dns<ResourceRecord>(raw, hdr.an_count);
    authorities = decode_dns<ResourceRecord>(raw, hdr.ns_count);
    additional = decode_dns<ResourceRecord>(raw, hdr.ar_count);
  }

  Message&
  Message::nx()
  {
    hdr.rcode(bits::rcode_name_error);
    return *this;
  }

  Message&
  Message::servfail()
  {
    hdr.rcode(bits::rcode_servfail);
    return *this;
  }

  bstring_t
  Message::encode_dns() const
  {
    // strip off authorities and additional because we encode them wrong or something so here we
    // reconstruct the header we write so it is correct.
    MessageHeader _hdr{hdr};
    _hdr.qd_count = questions.size();
    _hdr.an_count = answers.size();
    _hdr.ns_count = 0;
    _hdr.ar_count = 0;

    _hdr.fields = bits::make_rcode(answers.empty() ? bits::rcode_name_error : bits::rcode_no_error);

    auto ret = _hdr.encode_dns();
    for (const auto& question : questions)
      ret += question.encode_dns();
    for (const auto& answer : answers)
      ret += answer.encode_dns();

    return ret;
  }
  util::StatusObject
  Message::to_json() const
  {
    std::vector<util::StatusObject> ques;
    std::vector<util::StatusObject> ans;
    for (const auto& q : questions)
    {
      ques.push_back(q.to_json());
    }
    for (const auto& a : answers)
    {
      ans.push_back(a.to_json());
    }
    return util::StatusObject{{"questions", ques}, {"answers", ans}};
  }

  std::string
  MessageHeader::ToString() const
  {
    return fmt::format(
        "[dns::MessageHeader id={:x} flags={:x} questions={:x} answers={:x} ns={:x} ar={:x}]"_format(
            id, fields, qd_count, an_count, ns_count, ar_count));
  }

  std::string
  Message::ToString() const
  {
    return fmt::format(
        "[dns::Message hdr={} questions={{{}}} answers={{{}}} authorities={{{}}} "
        "additional={{{}}}]",
        hdr,
        fmt::format("{}", fmt::join(questions, ",")),
        fmt::format("{}", fmt::join(answers, ",")),
        fmt::format("{}", fmt::join(authorities, ",")),
        fmt::format("{}", fmt::join(additional, ",")));
  }

}  // namespace llarp::dns
