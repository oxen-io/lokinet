#include "dnsd.hpp" // for llarp_handle_dnsd_recvfrom, dnsc
#include "logger.hpp"

uint16_t
get16bits(const char *&buffer) throw()
{
  uint16_t value = static_cast< unsigned char >(buffer[0]);
  value     = value << 8;
  value += static_cast< unsigned char >(buffer[1]);
  buffer += 2;
  return value;
}

// uint32_t
uint32_t
get32bits(const char *&buffer) throw()
{
  uint32_t value = uint32_t(
      (unsigned char)(buffer[0]) << 24 | (unsigned char)(buffer[1]) << 16
      | (unsigned char)(buffer[2]) << 8 | (unsigned char)(buffer[3]));
  buffer += 4;
  return value;
}

dns_msg_header *
decode_hdr(const char *buffer)
{
  dns_msg_header *hdr = new dns_msg_header;
  hdr->id             = get16bits(buffer);
  uint fields         = get16bits(buffer);
  uint8_t lFields = (fields & 0x00FF) >> 0;
  uint8_t hFields = (fields & 0xFF00) >> 8;
  // hdr->qr      = fields & 0x8000;
  hdr->qr     = (hFields >> 7) & 0x1;
  hdr->opcode = fields & 0x7800;
  hdr->aa     = fields & 0x0400;
  hdr->tc     = fields & 0x0200;
  hdr->rd     = fields & 0x0100;

  hdr->ra     = (lFields >> 7) & 0x1;
  //hdr->z       = (lFields >> 6) & 0x1;
  //hdr->ad      = (lFields >> 5) & 0x1;
  //hdr->cd      = (lFields >> 4) & 0x1;
  hdr->rcode   = lFields & 0xf;

  hdr->qdCount = get16bits(buffer);
  hdr->anCount = get16bits(buffer);
  hdr->nsCount = get16bits(buffer);
  hdr->arCount = get16bits(buffer);
  return hdr;
}

dns_msg_question *
decode_question(const char *buffer)
{
  dns_msg_question *question = new dns_msg_question;
  std::string m_qName        = "";
  int length                 = *buffer++;
  // llarp::LogInfo("qNamLen", length);
  while(length != 0)
  {
    for(int i = 0; i < length; i++)
    {
      char c = *buffer++;
      m_qName.append(1, c);
    }
    length = *buffer++;
    if(length != 0)
      m_qName.append(1, '.');
  }
  question->name   = m_qName;
  question->type   = get16bits(buffer);
  question->qClass = get16bits(buffer);
  return question;
}

dns_msg_answer *
decode_answer(const char *buffer)
{
  dns_msg_answer *answer = new dns_msg_answer;
  answer->type           = get16bits(buffer);
  //assert(answer->type < 259);
  if (answer->type > 259)
  {
    llarp::LogWarn("Answer type is off the charts");
  }
  answer->aClass         = get16bits(buffer);
  answer->ttl            = get32bits(buffer);
  answer->rdLen          = get16bits(buffer);
  if (answer->rdLen == 4)
  {
    answer->rData          = new uint8_t[answer->rdLen];
    memcpy(answer->rData, buffer, answer->rdLen);
  }
  else
  {
    llarp::LogWarn("Unknown Type ", answer->type);
  }
  return answer;
}

void
put16bits(char *&buffer, uint16_t value) throw()
{
  buffer[0] = (value & 0xFF00) >> 8;
  buffer[1] = value & 0xFF;
  buffer += 2;
}

void
put32bits(char *&buffer, uint32_t value) throw()
{
  buffer[0] = (value & 0xFF000000) >> 24;
  buffer[1] = (value & 0x00FF0000) >> 16;
  buffer[2] = (value & 0x0000FF00) >> 8;
  buffer[3] = (value & 0x000000FF) >> 0;
  buffer += 4;
}

void
code_domain(char *&buffer, const std::string &domain) throw()
{
  int start(0), end;  // indexes
  // llarp::LogInfo("domain [", domain, "]");
  while((end = domain.find('.', start)) != std::string::npos)
  {
    *buffer++ = end - start;  // label length octet
    for(int i = start; i < end; i++)
    {
      *buffer++ = domain[i];  // label octets
      // llarp::LogInfo("Writing ", domain[i], " at ", i);
    }
    start = end + 1;  // Skip '.'
  }

  // llarp::LogInfo("start ", start, " domain size ", domain.size());

  *buffer++ = domain.size() - start;  // last label length octet
  for(int i = start; i < domain.size(); i++)
  {
    *buffer++ = domain[i];  // last label octets
    // llarp::LogInfo("Writing ", domain[i], " at ", i);
  }

  *buffer++ = 0;
}

void
llarp_handle_dns_recvfrom(struct llarp_udp_io *udp,
                          const struct sockaddr *saddr, const void *buf,
                          ssize_t sz)
{
  unsigned char *castBuf = (unsigned char *)buf;
  // auto buffer = llarp::StackBuffer< decltype(castBuf) >(castBuf);
  dns_msg_header *hdr = decode_hdr((const char *)castBuf);
  // castBuf += 12;
  llarp::LogDebug("msg id ", hdr->id);
  llarp::LogDebug("msg qr ", (uint8_t)hdr->qr);
  if(hdr->qr)
  {
    llarp::LogDebug("handling as dnsc answer");
    llarp_handle_dnsc_recvfrom(udp, saddr, buf, sz);
  }
  else
  {
    llarp::LogDebug("handling as dnsd question");
    llarp_handle_dnsd_recvfrom(udp, saddr, buf, sz);
  }
  /*
   llarp::LogInfo("msg op ", hdr->opcode);
   llarp::LogInfo("msg rc ", hdr->rcode);

   for(uint i = 0; i < hdr->qdCount; i++)
   {
   dns_msg_question *question = decode_question((const char*)castBuf);
   llarp::LogInfo("Read a question");
   castBuf += question->name.length() + 8;
   }

   for(uint i = 0; i < hdr->anCount; i++)
   {
   dns_msg_answer *answer = decode_answer((const char*)castBuf);
   llarp::LogInfo("Read an answer");
   castBuf += answer->name.length() + 4 + 4 + 4 + answer->rdLen;
   }
   */
}
