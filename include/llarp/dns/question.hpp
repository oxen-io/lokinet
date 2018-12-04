#ifndef LLARP_DNS_QUESTION_HPP
#define LLARP_DNS_QUESTION_HPP
#include <llarp/dns/serialize.hpp>
#include <llarp/dns/name.hpp>
#include <llarp/net_int.hpp>

namespace llarp
{
  namespace dns
  {
    using QType_t  = uint16_t;
    using QClass_t = uint16_t;

    struct Question : public Serialize
    {
      Question() = default;
      Question(Question&& other);
      Question(const Question& other);
      bool
      Encode(llarp_buffer_t* buf) const override;

      bool
      Decode(llarp_buffer_t* buf) override;

      bool
      operator==(const Question& other) const
      {
        return qname == other.qname && qtype == other.qtype
            && qclass == other.qclass;
      }

      friend std::ostream&
      operator<<(std::ostream& out, const Question& q)
      {
        return out << "qname=" << q.qname << " qtype=" << (int)q.qtype
                   << " qclass=" << (int)q.qclass;
      }

      Name_t qname;
      QType_t qtype;
      QClass_t qclass;
    };
  }  // namespace dns
}  // namespace llarp

#endif
