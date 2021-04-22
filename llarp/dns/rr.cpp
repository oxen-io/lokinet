#include "rr.hpp"
#include "dns.hpp"
#include <llarp/util/mem.hpp>
#include <llarp/util/logging/logger.hpp>
#include <llarp/util/printer.hpp>

namespace llarp
{
  namespace dns
  {
    ResourceRecord::ResourceRecord(const ResourceRecord& other)
        : rr_name(other.rr_name)
        , rr_type(other.rr_type)
        , rr_class(other.rr_class)
        , ttl(other.ttl)
        , rData(other.rData)
    {}

    ResourceRecord::ResourceRecord(ResourceRecord&& other)
        : rr_name(std::move(other.rr_name))
        , rr_type(std::move(other.rr_type))
        , rr_class(std::move(other.rr_class))
        , ttl(std::move(other.ttl))
        , rData(std::move(other.rData))
    {}

    bool
    ResourceRecord::Encode(llarp_buffer_t* buf) const
    {
      if (not EncodeName(buf, rr_name))
        return false;
      if (!buf->put_uint16(rr_type))
      {
        return false;
      }
      if (!buf->put_uint16(rr_class))
      {
        return false;
      }
      if (!buf->put_uint32(ttl))
      {
        return false;
      }
      if (!EncodeRData(buf, rData))
      {
        return false;
      }
      return true;
    }

    bool
    ResourceRecord::Decode(llarp_buffer_t* buf)
    {
      uint16_t discard;
      if (!buf->read_uint16(discard))
        return false;
      if (!buf->read_uint16(rr_type))
      {
        llarp::LogDebug("failed to decode rr type");
        return false;
      }
      if (!buf->read_uint16(rr_class))
      {
        llarp::LogDebug("failed to decode rr class");
        return false;
      }
      if (!buf->read_uint32(ttl))
      {
        llarp::LogDebug("failed to decode ttl");
        return false;
      }
      if (!DecodeRData(buf, rData))
      {
        llarp::LogDebug("failed to decode rr rdata ", *this);
        return false;
      }
      return true;
    }

    std::ostream&
    ResourceRecord::print(std::ostream& stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printAttribute("name", rr_name);
      printer.printAttribute("type", rr_type);
      printer.printAttribute("class", rr_class);
      printer.printAttribute("ttl", ttl);
      printer.printAttribute("rdata", rData.size());

      return stream;
    }

    bool
    ResourceRecord::HasCNameForTLD(const std::string& tld) const
    {
      if (rr_type != qTypeCNAME)
        return false;
      Name_t name;
      llarp_buffer_t buf(rData);
      if (not DecodeName(&buf, name))
        return false;
      return name.find(tld) != std::string::npos
          && name.rfind(tld) == (name.size() - tld.size()) - 1;
    }

  }  // namespace dns
}  // namespace llarp
