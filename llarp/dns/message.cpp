#include <dns/message.hpp>

#include <dns/dns.hpp>
#include <util/buffer.hpp>
#include <util/endian.hpp>
#include <util/logging/logger.hpp>
#include <util/printer.hpp>
#include <net/ip.hpp>

#include <array>

namespace llarp
{
  namespace dns
  {
    bool
    MessageHeader::Encode(llarp_buffer_t* buf) const
    {
      if(!buf->put_uint16(id))
        return false;
      if(!buf->put_uint16(fields))
        return false;
      if(!buf->put_uint16(qd_count))
        return false;
      if(!buf->put_uint16(an_count))
        return false;
      if(!buf->put_uint16(ns_count))
        return false;
      return buf->put_uint16(ar_count);
    }

    bool
    MessageHeader::Decode(llarp_buffer_t* buf)
    {
      if(!buf->read_uint16(id))
        return false;
      if(!buf->read_uint16(fields))
        return false;
      if(!buf->read_uint16(qd_count))
        return false;
      if(!buf->read_uint16(an_count))
        return false;
      if(!buf->read_uint16(ns_count))
        return false;
      if(!buf->read_uint16(ar_count))
        return false;
      return true;
    }

    Message::Message(Message&& other)
        : hdr_id(std::move(other.hdr_id))
        , hdr_fields(std::move(other.hdr_fields))
        , questions(std::move(other.questions))
        , answers(std::move(other.answers))
        , authorities(std::move(other.authorities))
        , additional(std::move(other.additional))
    {
    }

    Message::Message(const Message& other)
        : hdr_id(other.hdr_id)
        , hdr_fields(other.hdr_fields)
        , questions(other.questions)
        , answers(other.answers)
        , authorities(other.authorities)
        , additional(other.additional)
    {
    }

    Message::Message(const MessageHeader& hdr)
        : hdr_id(hdr.id), hdr_fields(hdr.fields)
    {
      questions.resize(size_t(hdr.qd_count));
      answers.resize(size_t(hdr.an_count));
      authorities.resize(size_t(hdr.ns_count));
      additional.resize(size_t(hdr.ar_count));
    }

    bool
    Message::Encode(llarp_buffer_t* buf) const
    {
      MessageHeader hdr;
      hdr.id       = hdr_id;
      hdr.fields   = hdr_fields;
      hdr.qd_count = questions.size();
      hdr.an_count = answers.size();
      hdr.ns_count = 0;
      hdr.ar_count = 0;

      if(!hdr.Encode(buf))
        return false;

      for(const auto& question : questions)
        if(!question.Encode(buf))
          return false;

      for(const auto& answer : answers)
        if(!answer.Encode(buf))
          return false;

      return true;
    }

    bool
    Message::Decode(llarp_buffer_t* buf)
    {
      for(auto& qd : questions)
      {
        if(!qd.Decode(buf))
        {
          llarp::LogError("failed to decode question");
          return false;
        }
        llarp::LogDebug(qd);
      }
      for(auto& an : answers)
      {
        if(not an.Decode(buf))
        {
          llarp::LogError("failed to decode answer");
          return false;
        }
      }
      return true;
    }

    void Message::AddServFail(RR_TTL_t)
    {
      if(questions.size())
      {
        hdr_fields |= flags_RCODEServFail;
        // authorative response with recursion available
        hdr_fields |= flags_QR | flags_AA | flags_RA;
        // don't allow recursion on this request
        hdr_fields &= ~flags_RD;
      }
    }

    static constexpr uint16_t
    reply_flags(uint16_t setbits)
    {
      return setbits | flags_QR | flags_AA | flags_RA;
    }

    void
    Message::AddINReply(llarp::huint128_t ip, bool isV6, RR_TTL_t ttl)
    {
      if(questions.size())
      {
        hdr_fields = reply_flags(hdr_fields);
        ResourceRecord rec;
        rec.rr_name  = questions[0].qname;
        rec.rr_class = qClassIN;
        rec.ttl      = ttl;
        if(isV6)
        {
          rec.rr_type = qTypeAAAA;
          ip.ToV6(rec.rData);
        }
        else
        {
          const auto addr = net::IPPacket::TruncateV6(ip);
          rec.rr_type     = qTypeA;
          rec.rData.resize(4);
          htobe32buf(rec.rData.data(), addr.h);
        }
        answers.emplace_back(std::move(rec));
      }
    }

    void
    Message::AddAReply(std::string name, RR_TTL_t ttl)
    {
      if(questions.size())
      {
        hdr_fields = reply_flags(hdr_fields);

        const auto& question = questions[0];
        answers.emplace_back();
        auto& rec                     = answers.back();
        rec.rr_name                   = question.qname;
        rec.rr_type                   = question.qtype;
        rec.rr_class                  = qClassIN;
        rec.ttl                       = ttl;
        std::array< byte_t, 512 > tmp = {{0}};
        llarp_buffer_t buf(tmp);
        if(EncodeName(&buf, name))
        {
          buf.sz = buf.cur - buf.base;
          rec.rData.resize(buf.sz);
          memcpy(rec.rData.data(), buf.base, buf.sz);
        }
      }
    }

    void
    Message::AddCNAMEReply(std::string name, RR_TTL_t ttl)
    {
      if(questions.size())
      {
        hdr_fields = reply_flags(hdr_fields);

        const auto& question = questions[0];
        answers.emplace_back();
        auto& rec                     = answers.back();
        rec.rr_name                   = question.qname;
        rec.rr_type                   = qTypeCNAME;
        rec.rr_class                  = qClassIN;
        rec.ttl                       = ttl;
        std::array< byte_t, 512 > tmp = {{0}};
        llarp_buffer_t buf(tmp);
        if(EncodeName(&buf, name))
        {
          buf.sz = buf.cur - buf.base;
          rec.rData.resize(buf.sz);
          memcpy(rec.rData.data(), buf.base, buf.sz);
        }
      }
    }

    void
    Message::AddMXReply(std::string name, uint16_t priority, RR_TTL_t ttl)
    {
      if(questions.size())
      {
        hdr_fields = reply_flags(hdr_fields);

        const auto& question = questions[0];
        answers.emplace_back();
        auto& rec                     = answers.back();
        rec.rr_name                   = question.qname;
        rec.rr_type                   = qTypeMX;
        rec.rr_class                  = qClassIN;
        rec.ttl                       = ttl;
        std::array< byte_t, 512 > tmp = {{0}};
        llarp_buffer_t buf(tmp);
        buf.put_uint16(priority);
        if(EncodeName(&buf, name))
        {
          buf.sz = buf.cur - buf.base;
          rec.rData.resize(buf.sz);
          memcpy(rec.rData.data(), buf.base, buf.sz);
        }
      }
    }

    void Message::AddNXReply(RR_TTL_t)
    {
      if(questions.size())
      {
        // authorative response with recursion available
        hdr_fields = reply_flags(hdr_fields);
        // don't allow recursion on this request
        hdr_fields &= ~flags_RD;
        hdr_fields |= flags_RCODENameError;
      }
    }

    std::ostream&
    Message::print(std::ostream& stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);

      printer.printAttributeAsHex("dns message id", hdr_id);
      printer.printAttributeAsHex("fields", hdr_fields);
      printer.printAttribute("questions", questions);
      printer.printAttribute("answers", answers);
      printer.printAttribute("nameserer", authorities);
      printer.printAttribute("additional", additional);

      return stream;
    }

  }  // namespace dns
}  // namespace llarp
