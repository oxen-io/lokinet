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
    };

    struct Message : public Serialize
    {
      void
      UpdateHeader();

      Message&
      AddNXReply();

      Message&
      AddINReply(llarp::huint32_t addr);

      Message&
      AddAReply(std::string name);

      bool
      Encode(llarp_buffer_t* buf) const override;

      bool
      Decode(llarp_buffer_t* buf) override;

      MessageHeader hdr;
      std::vector< Question > questions;
      std::vector< ResourceRecord > answers;
      std::vector< ResourceRecord > authorities;
      std::vector< ResourceRecord > additional;
    };
  }  // namespace dns
}  // namespace llarp

#endif
