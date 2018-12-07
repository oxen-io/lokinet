#include <llarp/dns/question.hpp>
#include <llarp/logger.hpp>

namespace llarp
{
  namespace dns
  {
    Question::Question(Question&& other)
        : qname(std::move(other.qname))
        , qtype(std::move(other.qtype))
        , qclass(std::move(other.qclass))
    {
    }
    Question::Question(const Question& other)
        : qname(other.qname), qtype(other.qtype), qclass(other.qclass)
    {
    }

    bool
    Question::Encode(llarp_buffer_t* buf) const
    {
      if(!EncodeName(buf, qname))
        return false;
      if(!llarp_buffer_put_uint16(buf, qtype))
        return false;
      return llarp_buffer_put_uint16(buf, qclass);
    }

    bool
    Question::Decode(llarp_buffer_t* buf)
    {
      if(!DecodeName(buf, qname))
      {
        llarp::LogError("failed to decode name");
        return false;
      }
      if(!llarp_buffer_read_uint16(buf, &qtype))
      {
        llarp::LogError("failed to decode type");
        return false;
      }
      if(!llarp_buffer_read_uint16(buf, &qclass))
      {
        llarp::LogError("failed to decode class");
        return false;
      }
      return true;
    }
  }  // namespace dns
}  // namespace llarp
