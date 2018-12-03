#include <llarp/dns/rr.hpp>

namespace llarp
{
  namespace dns
  {
    bool
    ResourceRecord::Encode(llarp_buffer_t* buf) const
    {
      if(!EncodeName(buf, rr_name))
        return false;
      if(!EncodeInt(buf, rr_type))
        return false;
      if(!EncodeInt(buf, rr_class))
        return false;
      if(!EncodeInt(buf, ttl))
        return false;
      return EncodeRData(buf, rData);
    }

    bool
    ResourceRecord::Decode(llarp_buffer_t* buf)
    {
      if(!DecodeName(buf, rr_name))
        return false;
      if(!DecodeInt(buf, rr_type))
        return false;
      if(!DecodeInt(buf, rr_class))
        return false;
      if(!DecodeInt(buf, ttl))
        return false;
      return DecodeRData(buf, rData);
    }
  }  // namespace dns
}  // namespace llarp
