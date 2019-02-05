#include <dns/message.hpp>

#include <dns/dns.hpp>
#include <util/buffer.hpp>
#include <util/endian.hpp>
#include <util/logger.hpp>

#include <array>

namespace llarp
{
  namespace dns
  {
    bool
    MessageHeader::Encode(llarp_buffer_t* buf) const
    {
      if(!llarp_buffer_put_uint16(buf, id))
        return false;
      if(!llarp_buffer_put_uint16(buf, fields))
        return false;
      if(!llarp_buffer_put_uint16(buf, qd_count))
        return false;
      if(!llarp_buffer_put_uint16(buf, an_count))
        return false;
      if(!llarp_buffer_put_uint16(buf, ns_count))
        return false;
      return llarp_buffer_put_uint16(buf, ar_count);
    }

    bool
    MessageHeader::Decode(llarp_buffer_t* buf)
    {
      if(!llarp_buffer_read_uint16(buf, &id))
        return false;
      if(!llarp_buffer_read_uint16(buf, &fields))
        return false;
      if(!llarp_buffer_read_uint16(buf, &qd_count))
        return false;
      if(!llarp_buffer_read_uint16(buf, &an_count))
        return false;
      if(!llarp_buffer_read_uint16(buf, &ns_count))
        return false;
      return llarp_buffer_read_uint16(buf, &ar_count);
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
        : hdr_id(hdr.id)
        , hdr_fields(hdr.fields)
        , questions(size_t(hdr.qd_count))
        , answers(size_t(hdr.an_count))
        , authorities(size_t(hdr.ns_count))
        , additional(size_t(hdr.ar_count))
    {
    }

    bool
    Message::Encode(llarp_buffer_t* buf) const
    {
      MessageHeader hdr;
      hdr.id       = hdr_id;
      hdr.fields   = hdr_fields;
      hdr.qd_count = questions.size();
      hdr.an_count = answers.size();
      hdr.ns_count = authorities.size();
      hdr.ar_count = additional.size();

      if(!hdr.Encode(buf))
        return false;

      for(const auto& question : questions)
        if(!question.Encode(buf))
          return false;

      for(const auto& answer : answers)
        if(!answer.Encode(buf))
          return false;

      for(const auto& auth : authorities)
        if(!auth.Encode(buf))
          return false;

      for(const auto& rr : additional)
        if(!rr.Encode(buf))
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
        if(!an.Decode(buf))
        {
          llarp::LogError("failed to decode answer");
          return false;
        }
        llarp::LogDebug(an);
      }

      for(auto& ns : authorities)
      {
        if(!ns.Decode(buf))
        {
          llarp::LogError("failed to decode authority");
          return false;
        }
        llarp::LogDebug(ns);
      }

      for(auto& ar : additional)
      {
        if(!ar.Decode(buf))
        {
          llarp::LogError("failed to decode additonal");
          return false;
        }
        llarp::LogDebug(ar);
      }
      return true;
    }

    void
    Message::AddINReply(llarp::huint32_t ip, RR_TTL_t ttl)
    {
      if(questions.size())
      {
        hdr_fields |= flags_QR | flags_AA;
        const auto& question = questions[0];
        ResourceRecord rec;
        rec.rr_name  = question.qname;
        rec.rr_type  = qTypeA;
        rec.rr_class = qClassIN;
        rec.ttl      = ttl;
        rec.rData.resize(4);
        htobe32buf(rec.rData.data(), ip.h);
        answers.emplace_back(std::move(rec));
      }
    }

    void
    Message::AddAReply(std::string name, RR_TTL_t ttl)
    {
      if(questions.size())
      {
        hdr_fields |= flags_QR | flags_AA;
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
        hdr_fields |= flags_QR | flags_AA;
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
        hdr_fields |= flags_QR | flags_AA;
        const auto& question = questions[0];
        answers.emplace_back();
        auto& rec                     = answers.back();
        rec.rr_name                   = question.qname;
        rec.rr_type                   = qTypeMX;
        rec.rr_class                  = qClassIN;
        rec.ttl                       = ttl;
        std::array< byte_t, 512 > tmp = {{0}};
        llarp_buffer_t buf(tmp);
        llarp_buffer_put_uint16(&buf, priority);
        if(EncodeName(&buf, name))
        {
          buf.sz = buf.cur - buf.base;
          rec.rData.resize(buf.sz);
          memcpy(rec.rData.data(), buf.base, buf.sz);
        }
      }
    }

    void
    Message::AddNXReply(RR_TTL_t ttl)
    {
      if(questions.size())
      {
        hdr_fields |= flags_QR | flags_AA;
        // don't allow recursion
        hdr_fields &= ~flags_RD;
        // don't advertise recurision
        hdr_fields &= ~flags_RA;
        const auto& question = questions[0];
        if(question.qtype != qTypeAAAA)
        {
          hdr_fields |= flags_RCODENameError;
          if(question.qtype == qTypeA)
          {
            answers.emplace_back();
            auto& nx    = answers.back();
            nx.rr_name  = question.qname;
            nx.rr_type  = question.qtype;
            nx.rr_class = question.qclass;
            nx.ttl      = ttl;
            nx.rData.resize(1);
            nx.rData.data()[0] = 0;
          }
        }
      }
    }

  }  // namespace dns
}  // namespace llarp
