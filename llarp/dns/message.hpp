#pragma once

#include <cstdint>
#include <llarp/util/buffer.hpp>
#include <llarp/util/str.hpp>
#include "rr.hpp"
#include "question.hpp"
#include "srv_data.hpp"
#include "bits.hpp"

namespace llarp::dns
{

  struct MessageHeader
  {
    static constexpr size_t Size = 12;

    MessageHeader() = default;
    MessageHeader(const MessageHeader&) = default;
    MessageHeader(MessageHeader&&) = default;

    MessageHeader&
    operator=(const MessageHeader&) = default;
    MessageHeader&
    operator=(MessageHeader&&) = default;

    /// construct from binary view.
    /// raw is modified to contain any remaining data.
    explicit MessageHeader(byte_view_t& raw);

    uint16_t id;
    uint16_t fields;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;

    bstring_t
    encode_dns() const;

    util::StatusObject
    to_json() const;

    /// set rcode.
    MessageHeader&
    rcode(uint16_t i);

    std::string
    ToString() const;

    bool
    operator==(const MessageHeader& other) const
    {
      return id == other.id && fields == other.fields && qd_count == other.qd_count
          && an_count == other.an_count && ns_count == other.ns_count && ar_count == other.ar_count;
    }
  };

  struct Message
  {
    /// constructs a dns message from a raw dns wire protocol message.
    /// raw is modified to contain any remaining data.
    explicit Message(byte_view_t& raw);

    Message() = default;
    Message(const Message&) = default;
    Message(Message&&) = default;

    Message&
    operator=(const Message&) = default;
    Message&
    operator=(Message&&) = default;

    /// mark as not found.
    Message&
    nx();

    /// mark as servfail.
    Message&
    servfail();

    /// encodes this dns message to dns wire protocol.
    bstring_t
    encode_dns() const;

    util::StatusObject
    to_json() const;

    // Wrapper around Encode that encodes into a new buffer and returns it
    [[nodiscard]] inline OwnedBuffer
    ToBuffer() const
    {
      const auto data = encode_dns();
      return OwnedBuffer{data.data(), data.size()};
    }

    std::string
    ToString() const;

    MessageHeader hdr;
    std::vector<Question> questions;
    std::vector<ResourceRecord> answers;
    std::vector<ResourceRecord> authorities;
    std::vector<ResourceRecord> additional;

    template <typename... Args_t>
    void
    add_reply(RRType rr_type, Args_t&&... args)
    {
      auto qname = questions[0].qname();
      answers.emplace_back(qname, rr_type, args...);
    }

    inline void
    AddNSReply(std::string_view txt, uint32_t ttl = 1) [[deprecated("use rr_ns")]]
    {
      // todo
    }

    inline void
    AddTXTReply(std::string_view txt, uint32_t ttl = 1) [[deprecated("use rr_txt")]]
    {
      // todo
    }

    inline void
    AddMXReply(std::string_view name, uint32_t ttl = 1) [[deprecated("use rr_mx")]]
    {
      // todo
    }

    inline void
    AddCNAMEReply(std::string_view name, uint32_t ttl = 1) [[deprecated("use rr_cname")]]
    {
      add_reply(RRType::CNAME, RData{split_dns_name(name)}, ttl);
    }

    inline void
    AddPTRReply(std::string_view name, uint32_t ttl = 1) [[deprecated("use rr_ptr")]]
    {
      add_reply(RRType::PTR, RData{split_dns_name(name)}, ttl);
    }

    inline void
    AddAReply(net::ipv4addr_t ip, uint32_t ttl = 1) [[deprecated("use rr_cname")]]
    {
      add_reply(RRType::A, RData{ip}, ttl);
    }

    inline void
    AddSRVReply(const std::vector<dns::SRVData>& recs, uint32_t ttl = 1)
        [[deprecated("use rr_srv")]]
    {
      for (const auto& rec : recs)
        add_reply(RRType::SRV, RData{rec.encode_dns(questions[0].qname())}, ttl);
    }

    inline void
    rr_add_ipaddr(net::ipaddr_t ip, uint32_t ttl = 1)
    {
      add_reply(
          std::holds_alternative<net::ipv4addr_t>(ip) ? RRType::A : RRType::AAAA,
          var::visit([](auto&& ip) { return RData{ip}; }, ip),
          ttl);
    }

    inline void
    AddINReply(std::string_view addr, uint32_t ttl = 1) [[deprecated("use rr_in")]]
    {}
  };

}  // namespace llarp::dns

namespace llarp
{
  template <>
  constexpr inline bool IsToStringFormattable<llarp::dns::Message> = true;

  template <>
  constexpr inline bool IsToStringFormattable<llarp::dns::MessageHeader> = true;
}  // namespace llarp
