#pragma once

#include "name.hpp"
#include "serialize.hpp"
#include <llarp/net/net_int.hpp>

#include <memory>
#include <vector>

namespace llarp
{
  namespace dns
  {
    using RRClass_t = uint16_t;
    using RRType_t = uint16_t;
    using RR_RData_t = std::vector<byte_t>;
    using RR_TTL_t = uint32_t;

    struct ResourceRecord : public Serialize
    {
      ResourceRecord() = default;
      ResourceRecord(const ResourceRecord& other);
      ResourceRecord(ResourceRecord&& other);

      bool
      Encode(llarp_buffer_t* buf) const override;

      bool
      Decode(llarp_buffer_t* buf) override;

      std::ostream&
      print(std::ostream& stream, int level, int spaces) const;

      bool
      HasCNameForTLD(const std::string& tld) const;

      Name_t rr_name;
      RRType_t rr_type;
      RRClass_t rr_class;
      RR_TTL_t ttl;
      RR_RData_t rData;
    };

    inline std::ostream&
    operator<<(std::ostream& out, const ResourceRecord& rr)
    {
      return rr.print(out, -1, -1);
    }
  }  // namespace dns
}  // namespace llarp
