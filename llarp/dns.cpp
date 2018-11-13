#include <llarp/endian.h>
#include <llarp/dnsd.hpp>  // for llarp_handle_dnsd_recvfrom, dnsc
#include <llarp/logger.hpp>

void
hexDump(const char *buffer, uint16_t size)
{
  // would rather heap allocate than use VLA
  char *hex_buffer     = new char[size * 3 + 1];
  hex_buffer[size * 3] = 0;
  for(unsigned int j = 0; j < size; j++)
    sprintf(&hex_buffer[3 * j], "%02X ", buffer[j]);
  std::string str(hex_buffer);
  llarp::LogInfo("First ", size, " bytes: ", str);
  delete[] hex_buffer;
}

void
hexDumpAt(const char *const buffer, uint32_t pos, uint16_t size)
{
  // would rather heap allocate than use VLA
  char *hex_buffer     = new char[size * 3 + 1];
  hex_buffer[size * 3] = 0;
  for(unsigned int j = 0; j < size; j++)
    sprintf(&hex_buffer[3 * j], "%02X ", buffer[pos + j]);
  std::string str(hex_buffer);
  llarp::LogInfo(pos, " ", size, " bytes: ", str);
  delete[] hex_buffer;
}

/*
 <domain-name> is a domain name represented as a series of labels, and
 terminated by a label with zero length.  <character-string> is a single
 length octet followed by that number of characters.  <character-string>
 is treated as binary information, and can be up to 256 characters in
 length (including the length octet).
 */
std::string
getDNSstring(const char *const buffer, uint32_t *pos)
{
  std::string str      = "";
  const char *moveable = buffer;
  moveable += *pos;
  uint8_t length = *moveable++;
  (*pos)++;
  if(length == 0xc0)
  {
    uint8_t cPos = *moveable++;
    (*pos)++;
    uint32_t cPos32 = cPos;
    // llarp::LogInfo("Found reference at ", std::to_string(cPos));
    return getDNSstring(buffer, &cPos32);
  }
  // hexDump(moveable, length);
  // hexDump(buffer, length);
  // printf("dnsStringLen[%d]\n", length);
  // llarp::LogInfo("dnsStringLen ", std::to_string(length));
  if(!length)
    return str;
  while(length != 0)
  {
    // llarp::LogInfo("Reading ", std::to_string(length));
    for(int i = 0; i < length; i++)
    {
      char c = *moveable++;
      (*pos)++;
      str.append(1, c);
    }
    if(*moveable == '\xc0')
    {
      moveable++;
      (*pos)++;                     // forward one char
      uint8_t cPos    = *moveable;  // read char
      uint32_t cPos32 = cPos;
      // llarp::LogInfo("Remaining [", str, "] is an reference at ", (int)cPos);
      std::string addl = getDNSstring(buffer, &cPos32);
      // llarp::LogInfo("Addl str [", addl, "]");
      str += "." + addl;
      // llarp::LogInfo("Final str [", str, "]");
      (*pos)++;  // move past reference

      return str;
    }
    length = *moveable++;
    (*pos)++;
    if(length > 64)
      llarp::LogError("bug detected");
    // else
    // hexDump(buffer, length);
    // llarp::LogInfo("NextLen ", std::to_string(length));
    if(length != 0)
      str.append(1, '.');
  }
  // llarp::LogInfo("Returning ", str);
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
  decode_question(const char *buffer, uint32_t *pos)
  {
    // char *start = (char *)buffer;
    dns_msg_question *question = new dns_msg_question;

    // uint32_t start = *pos;
    std::string m_qName = getDNSstring(buffer, pos);
    llarp::LogDebug("Got question name: ", m_qName);
    // llarp::LogInfo("Started at ", std::to_string(start), " ended at: ",
    // std::to_string(*pos)); llarp::LogInfo("Advancing question buffer by ",
    // std::to_string(*pos)); buffer += (*pos) - start; buffer +=
    // m_qName.length() + 2;  // + length byte & ending terminator

    const char *moveable = buffer;
    moveable += *pos;  // advance to position
    // hexDump(moveable, 4);

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
    question->type = get16bits(moveable);
    (*pos) += 2;
    // printf("Now1 at [%d]\n", buffer - start);
    question->qClass = get16bits(moveable);
    (*pos) += 2;
    // printf("Now2 at [%d]\n", buffer - start);
    /*
    llarp::LogDebug("Type ", std::to_string(question->type), " Class ",
                    std::to_string(question->qClass));
                    */
    // hexDump(moveable, 4);
    return question;
  }

  dns_msg_answer *
  decode_answer(const char *const buffer, uint32_t *pos)
  {
    dns_msg_answer *answer = new dns_msg_answer;
    /*
    llarp_buffer_t bob;
    bob.base = (unsigned char *)buffer;
    bob.sz = 12;
    llarp::DumpBuffer(bob);
    */

    const char *moveable = buffer;
    // llarp::LogDebug("Advancing to pos ", std::to_string(*pos));
    moveable += (*pos);  // advance to position

    if(*moveable == '\xc0')
    {
      // hexDump(moveable, 12);
      // hexDumpAt(buffer, *pos, 12);

      // hexDump(moveable, 2);

      moveable++;
      (*pos)++;
      uint8_t readAt    = *moveable;
      uint32_t readAt32 = readAt;
      // llarp::LogInfo("Ref, skip. Read ", readAt32);
      // hexDumpAt(buffer, readAt, 2);
      answer->name = getDNSstring(buffer, &readAt32);
      moveable++;
      (*pos)++;
      // hexDump(moveable, 10);
      // hexDumpAt(buffer, *pos, 10);
    }
    else
    {
      // get DNSString?
      llarp::LogWarn("Need to parse string, implement me");
      /*
      uint32_t readAt32 = *pos;
      answer->name = getDNSstring(buffer, &readAt32);
        llarp::LogInfo("Parsed string ", answer->name, " read ", std::to_string(readAt32));
        moveable += readAt32; (*pos) += readAt32;
      */
      //moveable++; (*pos)++;
    }
    /*
    hexDump(moveable, 10);
    uint8_t first = *moveable++; (*pos)++;
    llarp::LogInfo("decode - first", std::to_string(first));
    // SOA hack
    if(first != 12 && first != 14)  // 0x0c (c0 0c) 0
    {
      llarp::LogDebug("decode - first isnt 12, stepping back");
      moveable--; (*pos)--; // rewind buffer one byte
    }
    */
    // hexDump(moveable, 10);

    answer->type = get16bits(moveable);
    (*pos) += 2;
    llarp::LogDebug("Answer Type: ", answer->type);
    // assert(answer->type < 259);
    if(answer->type > 259)
    {
      llarp::LogWarn("Answer type is off the charts");
    }
    answer->aClass = get16bits(moveable);
    (*pos) += 2;
    llarp::LogDebug("Answer Class: ", answer->aClass);
    answer->ttl = get32bits(moveable);
    (*pos) += 4;
    llarp::LogDebug("Answer TTL: ", answer->ttl);
    answer->rdLen = get16bits(moveable);
    (*pos) += 2;
    /*
    llarp::LogDebug("Answer rdL: ", answer->rdLen, " at ",
                    std::to_string(*pos));
                    */
    // uint32_t cPos = moveable - buffer;
    // llarp::LogInfo("pos at ", std::to_string(*pos), " calculated: ",
    // std::to_string(cPos));

    // hexDump(moveable, answer->rdLen);
    // hexDumpAt(buffer, *pos, answer->rdLen);
    if(answer->rdLen == 4)
    {
      answer->rData.resize(answer->rdLen);
      memcpy(answer->rData.data(), moveable, answer->rdLen);
      /*
      llarp::LogDebug("Read ", std::to_string(answer->rData[0]), ".",
                      std::to_string(answer->rData[1]), ".",
                      std::to_string(answer->rData[2]), ".",
                      std::to_string(answer->rData[3]));
                      */
      moveable += answer->rdLen;
      (*pos) += answer->rdLen;  // advance the length
    }
    else
    {
      llarp::LogDebug("Got type ", answer->type);
      // FIXME: move this out of here, this shouldn't be responsible for decode
      switch(answer->type)
      {
        case 5:
          moveable += answer->rdLen;
          (*pos) += answer->rdLen;  // advance the length
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
          moveable += answer->rdLen;
          (*pos) += answer->rdLen;  // advance the length
        }
        break;
        case 12:
        {
          std::string revname = getDNSstring(buffer, pos);
          llarp::LogInfo("revDNSname: ", revname);
          //answer->rData = new uint8_t[answer->rdLen + 1];
          answer->rData.resize(answer->rdLen);
          memcpy(answer->rData.data(), revname.c_str(), answer->rdLen);
          //answer->rData = (uint8_t *)strdup(revname.c_str()); // safer? nope
          moveable += answer->rdLen;
          //(*pos) += answer->rdLen;  // advance the length
        }
        break;
        case 15:
        {
          uint16_t priority = get16bits(moveable);
          (*pos) += 2;
          std::string revname = getDNSstring(buffer, pos);
          llarp::LogInfo("MX: ", revname, " @ ", priority);
          // answer->rData = new uint8_t[revname.length() + 1];
          // memcpy(answer->rData, revname.c_str(), answer->rdLen);
          answer->rData.resize(revname.size());
          memcpy(answer->rData.data(), revname.c_str(), revname.size());
          moveable += answer->rdLen - 2;  // advance the length
          // llarp::LogInfo("leaving at ", std::to_string(*pos));
          // hexDumpAt(buffer, *pos, 5);
          // hexDump(moveable, 5);
        }
        break;
        case 16:
        {
          // hexDump(buffer, 5);
          // std::string revname = getDNSstring((char *)buffer);
          llarp::LogInfo("TXT: size: ", answer->rdLen);
          answer->rData.resize(answer->rdLen);
          memcpy(answer->rData.data(), moveable + 1, answer->rdLen);
          moveable += answer->rdLen;
          (*pos) += answer->rdLen;  // advance the length
        }
        break;
        // type 28 AAAA
        default:
          moveable += answer->rdLen;
          (*pos) += answer->rdLen;  // advance the length
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
