#include <dns/rr.hpp>

#include <util/logging/logger.hpp>
#include <util/printer.hpp>

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
    {
    }

    ResourceRecord::ResourceRecord(ResourceRecord&& other)
        : rr_name(std::move(other.rr_name))
        , rr_type(std::move(other.rr_type))
        , rr_class(std::move(other.rr_class))
        , ttl(std::move(other.ttl))
        , rData(std::move(other.rData))
    {
    }

    bool
    ResourceRecord::Encode(llarp_buffer_t* buf) const
    {
      if(!EncodeName(buf, rr_name))
      {
        return false;
      }
      if(!buf->put_uint16(rr_type))
      {
        return false;
      }
      if(!buf->put_uint16(rr_class))
      {
        return false;
      }
      if(!buf->put_uint32(ttl))
      {
        return false;
      }
      if(!EncodeRData(buf, rData))
      {
        return false;
      }
      return true;
    }

    bool
    ResourceRecord::Decode(llarp_buffer_t* buf)
    {
      if(!DecodeName(buf, rr_name))
      {
        llarp::LogError("failed to decode rr name");
        return false;
      }
      if(!buf->read_uint16(rr_type))
      {
        llarp::LogError("failed to decode rr type");
        return false;
      }
      if(!buf->read_uint16(rr_class))
      {
        llarp::LogError("failed to decode rr class");
        return false;
      }
      if(!buf->read_uint32(ttl))
      {
        llarp::LogError("failed to decode ttl");
        return false;
      }
      if(!DecodeRData(buf, rData))
      {
        llarp::LogError("failed to decode rr rdata");
        return false;
      }
      return true;
    }

    std::ostream&
    ResourceRecord::print(std::ostream& stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printAttribute("RR name", rr_name);
      printer.printAttribute("type", rr_type);
      printer.printAttribute("class", rr_class);
      printer.printAttribute("ttl", ttl);
      printer.printAttribute("rdata", rData.size());

      return stream;
    }
  }  // namespace dns
}  // namespace llarp
