#include <llarp/endian.h>
#include <llarp/dnsd.hpp>  // for llarp_handle_dnsd_recvfrom, dnsc
#include <llarp/logger.hpp>

/*
 <domain-name> is a domain name represented as a series of labels, and
 terminated by a label with zero length.  <character-string> is a single
 length octet followed by that number of characters.  <character-string>
 is treated as binary information, and can be up to 256 characters in
 length (including the length octet).
 */
std::string
getDNSstring(const char *buffer)
{
  std::string str = "";
  uint8_t length  = *buffer++;
  // printf("dnsStringLen[%d]\n", length);
  // llarp::LogInfo("dnsStringLen ", length);
  if(!length)
    return str;
  while(length != 0)
  {
    for(int i = 0; i < length; i++)
    {
      char c = *buffer++;
      str.append(1, c);
    }
    length = *buffer++;
    if(length != 0)
      str.append(1, '.');
  }
  return str;
}

void
code_domain(char *&buffer, const std::string &domain) throw()
{
  std::string::size_type start(0);
  std::string::size_type end;  // indexes
  // llarp::LogInfo("domain [", domain, "]");
  while((end = domain.find('.', start)) != std::string::npos)
  {
    *buffer++ = end - start;  // label length octet
    for(std::string::size_type i = start; i < end; i++)
    {
      *buffer++ = domain[i];  // label octets
      // llarp::LogInfo("Writing ", domain[i], " at ", i);
    }
    start = end + 1;  // Skip '.'
  }

  // llarp::LogInfo("start ", start, " domain size ", domain.size());

  *buffer++ = domain.size() - start;  // last label length octet
  for(size_t i = start; i < domain.size(); i++)
  {
    *buffer++ = domain[i];  // last label octets
    // llarp::LogInfo("Writing ", domain[i], " at ", i);
  }

  *buffer++ = 0;
}

// lets just remove uint
#ifdef _WIN32
#define uint UINT
#endif

extern "C"
{
  uint16_t
  get16bits(const char *&buffer) throw()
  {
    uint16_t value = bufbe16toh(buffer);
    buffer += 2;
    return value;
  }

  uint32_t
  get32bits(const char *&buffer) throw()
  {
    uint32_t value = bufbe32toh(buffer);
    buffer += 4;
    return value;
  }

  dns_msg_header *
  decode_hdr(const char *buffer)
  {
    dns_msg_header *hdr = new dns_msg_header;
    hdr->id             = get16bits(buffer);
    uint16_t fields     = get16bits(buffer);
    uint8_t lFields     = (fields & 0x00FF) >> 0;
    uint8_t hFields     = (fields & 0xFF00) >> 8;
    // hdr->qr      = fields & 0x8000;
    hdr->qr     = (hFields >> 7) & 0x1;
    hdr->opcode = fields & 0x7800;
    hdr->aa     = fields & 0x0400;
    hdr->tc     = fields & 0x0200;
    hdr->rd     = fields & 0x0100;

    hdr->ra    = (lFields >> 7) & 0x1;
    hdr->z     = (lFields >> 6) & 0x1;
    hdr->ad    = (lFields >> 5) & 0x1;
    hdr->cd    = (lFields >> 4) & 0x1;
    hdr->rcode = lFields & 0xf;

    hdr->qdCount = get16bits(buffer);
    hdr->anCount = get16bits(buffer);
    hdr->nsCount = get16bits(buffer);
    hdr->arCount = get16bits(buffer);
    return hdr;
  }

  dns_msg_question *
  decode_question(const char *buffer)
  {
    // char *start = (char *)buffer;
    dns_msg_question *question = new dns_msg_question;
    std::string m_qName        = getDNSstring(buffer);
    buffer += m_qName.length() + 2;  // + length byte & ending terminator
    // printf("Now0 at [%d]\n", buffer - start);
    // buffer += m_qName.size() + 1;
    /*
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
    */
    question->name = m_qName;
    question->type = get16bits(buffer);
    // printf("Now1 at [%d]\n", buffer - start);
    question->qClass = get16bits(buffer);
    // printf("Now2 at [%d]\n", buffer - start);
    return question;
  }

  dns_msg_answer *
  decode_answer(const char *buffer)
  {
    dns_msg_answer *answer = new dns_msg_answer;
    // skip for now until we can handle compressed labels
    /*
    std::string aName      = getDNSstring((char *)buffer);
    buffer += aName.size() + 1;
    */

    /*
    llarp_buffer_t bob;
    bob.base = (unsigned char *)buffer;
    bob.sz = 12;
    llarp::DumpBuffer(bob);
    */

    char hex_buffer[12 * 3 + 1];
    hex_buffer[12 * 3] = 0;
    for(unsigned int j = 0; j < 10; j++)
      sprintf(&hex_buffer[3 * j], "%02X ", buffer[j]);
    llarp::LogDebug("First 12 bytes: ", hex_buffer);

    uint8_t first = *buffer++;
    // llarp::LogInfo("decode - first", std::to_string(first));
    // SOA hack
    if(first != 12 && first != 14)  // 0x0c (c0 0c) 0
    {
      llarp::LogDebug("decode - first isnt 12, stepping back");
      buffer--;  // rewind buffer one byte
    }
    answer->type = get16bits(buffer);
    // assert(answer->type < 259);
    if(answer->type > 259)
    {
      llarp::LogWarn("Answer type is off the charts");
    }
    answer->aClass = get16bits(buffer);
    answer->ttl    = get32bits(buffer);
    answer->rdLen  = get16bits(buffer);
    if(answer->rdLen == 4)
    {
      answer->rData = new uint8_t[answer->rdLen];
      memcpy(answer->rData, buffer, answer->rdLen);
      buffer += answer->rdLen;  // advance the length
    }
    else
    {
      llarp::LogDebug("Got type ", answer->type);
      switch(answer->type)
      {
        case 5:
          buffer += answer->rdLen;  // advance the length
          break;
        case 6:  // type 6 = SOA
        {
          // 2 names, then 4x 32bit
          // why risk any crashes
          if(answer->rdLen < 24)
          {
            llarp::LogWarn("Weird SOA is less than 24 bytes: ", answer->rdLen);
          }
          /*
          std::string mname = getDNSstring((char *)buffer);
          std::string rname = getDNSstring((char *)buffer);
          uint32_t serial   = get32bits(buffer);
          uint32_t refresh  = get32bits(buffer);
          uint32_t retry    = get32bits(buffer);
          uint32_t expire   = get32bits(buffer);
          uint32_t minimum  = get32bits(buffer);
          llarp::LogInfo("mname   : ", mname);
          llarp::LogInfo("rname   : ", rname);
          llarp::LogDebug("serial  : ", serial);
          llarp::LogDebug("refresh : ", refresh);
          llarp::LogDebug("retry   : ", retry);
          llarp::LogDebug("expire  : ", expire);
          llarp::LogDebug("minimum : ", minimum);
          */
        }
        break;
        case 12:
        {
          std::string revname = getDNSstring((char *)buffer);
          llarp::LogInfo("revDNSname: ", revname);
          answer->rData = new uint8_t[answer->rdLen + 1];
          memcpy(answer->rData, revname.c_str(), answer->rdLen);
        }
        break;
        default:
          buffer += answer->rdLen;  // advance the length
          llarp::LogWarn("Unknown Type ", answer->type);
          break;
      }
    }
    return answer;
  }

  void
  put16bits(char *&buffer, uint16_t value) throw()
  {
    htobe16buf(buffer, value);
    buffer += 2;
  }

  void
  put32bits(char *&buffer, uint32_t value) throw()
  {
    htobe32buf(buffer, value);
    buffer += 4;
  }

  void
  llarp_handle_dns_recvfrom(struct llarp_udp_io *udp,
                            const struct sockaddr *addr, const void *buf,
                            ssize_t sz)
  {
    unsigned char *castBuf = (unsigned char *)buf;
    // auto buffer = llarp::StackBuffer< decltype(castBuf) >(castBuf);
    dns_msg_header *hdr = decode_hdr((const char *)castBuf);
    // castBuf += 12;
    llarp::LogDebug("msg id ", hdr->id);
    llarp::LogDebug("msg qr ", (uint8_t)hdr->qr);
    if(!udp)
    {
      llarp::LogError("no udp passed in to handler");
    }
    if(!addr)
    {
      llarp::LogError("no source addr passed in to handler");
    }
    if(hdr->qr)
    {
      llarp::LogDebug("handling as dnsc answer");
      llarp_handle_dnsc_recvfrom(udp, addr, buf, sz);
    }
    else
    {
      llarp::LogDebug("handling as dnsd question");
      llarp_handle_dnsd_recvfrom(udp, addr, buf, sz);
    }
    delete hdr;
    /*
     llarp::LogInfo("msg op ", hdr->opcode);
     llarp::LogInfo("msg rc ", hdr->rcode);

     for(uint8_t i = 0; i < hdr->qdCount; i++)
     {
     dns_msg_question *question = decode_question((const char*)castBuf);
     llarp::LogInfo("Read a question");
     castBuf += question->name.length() + 8;
     }

     for(uint8_t i = 0; i < hdr->anCount; i++)
     {
     dns_msg_answer *answer = decode_answer((const char*)castBuf);
     llarp::LogInfo("Read an answer");
     castBuf += answer->name.length() + 4 + 4 + 4 + answer->rdLen;
     }
     */
  }
}
