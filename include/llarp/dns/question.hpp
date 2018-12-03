#ifndef LLARP_DNS_QUESTION_HPP
#define LLARP_DNS_QUESTION_HPP
#include <llarp/dns/serialize.hpp>
#include <llarp/dns/name.hpp>
#include <llarp/net_int.hpp>

namespace llarp
{
  namespace dns
  {
    using QType_t  = llarp::huint16_t;
    using QClass_t = llarp::huint16_t;

    struct Question : public Serialize
    {
      bool
      Encode(llarp_buffer_t* buf) const override;

      bool
      Decode(llarp_buffer_t* buf) override;

      Name_t qname;
      QType_t qtype;
      QClass_t qclass;
    };
  }  // namespace dns
}  // namespace llarp

#endif
