#include <llarp/dns/message.hpp>
#include <llarp/endian.hpp>
namespace llarp
{
  namespace dns
  {
    bool
    MessageHeader::Encode(llarp_buffer_t* buf) const
    {
      if(!EncodeInt(buf, id))
        return false;
      if(!EncodeInt(buf, fields))
        return false;
      if(!EncodeInt(buf, qd_count))
        return false;
      if(!EncodeInt(buf, an_count))
        return false;
      if(!EncodeInt(buf, ns_count))
        return false;
      return EncodeInt(buf, ar_count);
    }

    bool
    MessageHeader::Decode(llarp_buffer_t* buf)
    {
      if(!DecodeInt(buf, id))
        return false;
      if(!DecodeInt(buf, fields))
        return false;
      if(!DecodeInt(buf, qd_count))
        return false;
      if(!DecodeInt(buf, an_count))
        return false;
      if(!DecodeInt(buf, ns_count))
        return false;
      return DecodeInt(buf, ar_count);
    }

    bool
    Message::Encode(llarp_buffer_t* buf) const
    {
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
      if(!hdr.Decode(buf))
        return false;
      questions.resize(hdr.qd_count);
      answers.resize(hdr.an_count);
      authorities.resize(hdr.ns_count);
      additional.resize(hdr.ar_count);

      for(auto& qd : questions)
        if(!qd.Decode(buf))
          return false;

      for(auto& an : answers)
        if(!an.Decode(buf))
          return false;

      for(auto& ns : authorities)
        if(!ns.Decode(buf))
          return false;

      for(auto& ar : additional)
        if(!ar.Decode(buf))
          return false;
      return true;
    }

    void
    Message::UpdateHeader()
    {
      hdr.qd_count = questions.size();
      hdr.an_count = answers.size();
      hdr.ns_count = authorities.size();
      hdr.ar_count = additional.size();
    }

    Message&
    Message::AddINReply(llarp::huint32_t ip)
    {
      if(questions.size())
      {
        hdr.fields |= (1 << 15);
        const auto& question = questions[0];
        answers.emplace_back();
        auto& rec     = answers.back();
        rec.rr_name   = question.qname;
        rec.rr_type.h = 1;
        rec.rr_class  = question.qclass;
        rec.ttl.h     = 1;
        rec.rData.resize(4);
        htobe32buf(rec.rData.data(), ip.h);
        UpdateHeader();
      }
      return *this;
    }

    Message&
    Message::AddAReply(std::string name)
    {
      if(questions.size())
      {
        hdr.fields |= (1 << 15);
        const auto& question = questions[0];
        answers.emplace_back();
        auto& rec      = answers.back();
        rec.rr_name    = question.qname;
        rec.rr_type    = question.qtype;
        rec.rr_class.h = 1;
        rec.ttl.h      = 1;
        rec.rData.resize(name.size());
        memcpy(rec.rData.data(), name.c_str(), rec.rData.size());
        UpdateHeader();
      }
      return *this;
    }

    Message&
    Message::AddNXReply()
    {
      if(questions.size())
      {
        hdr.fields |= (1 << 15) | (1 << 3);
        const auto& question = questions[0];
        answers.emplace_back();
        auto& nx    = answers.back();
        nx.rr_name  = question.qname;
        nx.rr_type  = question.qtype;
        nx.rr_class = question.qclass;
        nx.ttl.h    = 1;
        nx.rData.resize(1);
        nx.rData.data()[0] = 0;
        UpdateHeader();
      }
      return *this;
    }

  }  // namespace dns
}  // namespace llarp
