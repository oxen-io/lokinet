#include <dns/question.hpp>

#include <util/logging/logger.hpp>
#include <util/printer.hpp>

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
      if (!EncodeName(buf, qname))
        return false;
      if (!buf->put_uint16(qtype))
        return false;
      return buf->put_uint16(qclass);
    }

    bool
    Question::Decode(llarp_buffer_t* buf)
    {
      if (!DecodeName(buf, qname))
      {
        llarp::LogError("failed to decode name");
        return false;
      }
      if (!buf->read_uint16(qtype))
      {
        llarp::LogError("failed to decode type");
        return false;
      }
      if (!buf->read_uint16(qclass))
      {
        llarp::LogError("failed to decode class");
        return false;
      }
      return true;
    }

    bool
    Question::IsName(const std::string& other) const
    {
      // does other have a . at the end?
      if (other.find_last_of('.') == (other.size() - 1))
        return other == qname;
      // no, add it and retry
      return IsName(other + ".");
    }

    std::string
    Question::Name() const
    {
      return qname.substr(0, qname.find_last_of('.'));
    }

    bool
    Question::HasTLD(const std::string& tld) const
    {
      return qname.find(tld) != std::string::npos
          && qname.rfind(tld) == (qname.size() - tld.size()) - 1;
    }

    std::ostream&
    Question::print(std::ostream& stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printAttribute("qname", qname);
      printer.printAttributeAsHex("qtype", qtype);
      printer.printAttributeAsHex("qclass", qclass);

      return stream;
    }
  }  // namespace dns
}  // namespace llarp
