#include <llarp/dns/question.hpp>

namespace llarp
{
  namespace dns
  {
    bool
    Question::Encode(llarp_buffer_t* buf) const
    {
      if(!EncodeName(buf, qname))
        return false;
      if(!EncodeInt(buf, qtype))
        return false;
      return EncodeInt(buf, qclass);
    }

    bool
    Question::Decode(llarp_buffer_t* buf)
    {
      if(!DecodeName(buf, qname))
        return false;
      if(!DecodeInt(buf, qtype))
        return false;
      return DecodeInt(buf, qclass);
    }
  }  // namespace dns
}  // namespace llarp
