#include <dns/question.hpp>

#include <util/logger.hpp>

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
      if(!buf->put_uint16(qtype))
        return false;
      return buf->put_uint16(qclass);
    }

    bool
    Question::Decode(llarp_buffer_t* buf)
    {
      if(!DecodeName(buf, qname))
      {
        llarp::LogError("failed to decode name");
        return false;
      }
      if(!buf->read_uint16(qtype))
      {
        llarp::LogError("failed to decode type");
        return false;
      }
      if(!buf->read_uint16(qclass))
      {
        llarp::LogError("failed to decode class");
        return false;
      }
      return true;
    }
  }  // namespace dns
}  // namespace llarp
