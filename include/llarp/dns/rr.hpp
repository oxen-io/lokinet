#ifndef LLARP_DNS_RR_HPP
#define LLARP_DNS_RR_HPP
#include <llarp/dns/name.hpp>
#include <llarp/dns/serialize.hpp>
#include <llarp/net_int.hpp>
#include <vector>
#include <memory>

namespace llarp
{
  namespace dns
  {
    using RRClass_t  = uint16_t;
    using RRType_t   = uint16_t;
    using RR_RData_t = std::vector< byte_t >;
    using RR_TTL_t   = uint32_t;

    struct ResourceRecord : public Serialize
    {
      ResourceRecord() = default;
      ResourceRecord(const ResourceRecord& other);
      ResourceRecord(ResourceRecord&& other);

      bool
      Encode(llarp_buffer_t* buf) const override;

      bool
      Decode(llarp_buffer_t* buf) override;

      friend std::ostream&
      operator<<(std::ostream& out, const ResourceRecord& rr)
      {
        return out << "[RR name=" << rr.rr_name << " type=" << rr.rr_type
                   << " class=" << rr.rr_class << " ttl=" << rr.ttl
                   << " rdata=[" << rr.rData.size() << " bytes]";
      }

      Name_t rr_name;
      RRType_t rr_type;
      RRClass_t rr_class;
      RR_TTL_t ttl;
      RR_RData_t rData;
    };
  }  // namespace dns
}  // namespace llarp

#endif
