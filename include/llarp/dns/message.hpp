#ifndef LLARP_DNS_MESSAGE_HPP
#define LLARP_DNS_MESSAGE_HPP
#include <llarp/dns/serialize.hpp>
#include <llarp/dns/rr.hpp>
#include <llarp/dns/question.hpp>

namespace llarp
{
  namespace dns
  {
    using MsgID_t  = uint16_t;
    using Fields_t = uint16_t;
    using Count_t  = uint16_t;

    struct MessageHeader : public Serialize
    {
      const static size_t Size = 12;

      MessageHeader() = default;

      MsgID_t id;
      Fields_t fields;
      Count_t qd_count;
      Count_t an_count;
      Count_t ns_count;
      Count_t ar_count;

      bool
      Encode(llarp_buffer_t* buf) const override;

      bool
      Decode(llarp_buffer_t* buf) override;

      bool
      operator==(const MessageHeader& other) const
      {
        return id == other.id && fields == other.fields
            && qd_count == other.qd_count && an_count == other.an_count
            && ns_count == other.ns_count && ar_count == other.ar_count;
      }
    };

    struct Message : public Serialize
    {
      Message(const MessageHeader& hdr);

      Message(Message&& other);
      Message(const Message& other);

      void
      UpdateHeader();

      void
      AddNXReply();

      void
      AddMXReply(std::string name, uint16_t priority);

      void
      AddINReply(llarp::huint32_t addr);

      void
      AddAReply(std::string name);

      bool
      Encode(llarp_buffer_t* buf) const override;

      bool
      Decode(llarp_buffer_t* buf) override;

      friend std::ostream&
      operator<<(std::ostream& out, const Message& msg)
      {
        out << "[dns message id=" << std::hex << msg.hdr_id
            << " fields=" << msg.hdr_fields << " questions=[ ";
        for(const auto& qd : msg.questions)
          out << qd << ", ";
        out << "] answers=[ ";
        for(const auto& an : msg.answers)
          out << an << ", ";
        out << "] nameserver=[ ";
        for(const auto& ns : msg.authorities)
          out << ns << ", ";
        out << "] additional=[ ";
        for(const auto& ar : msg.additional)
          out << ar << ", ";
        return out << "]";
      }

      MsgID_t hdr_id;
      Fields_t hdr_fields;
      std::vector< Question > questions;
      std::vector< ResourceRecord > answers;
      std::vector< ResourceRecord > authorities;
      std::vector< ResourceRecord > additional;
    };
  }  // namespace dns
}  // namespace llarp

#endif
