#include <dnsd.hpp>  // for llarp_handle_dnsd_recvfrom, dnsc

#include <util/buffer.hpp>
#include <util/endian.hpp>
#include <util/logger.hpp>

void
hexDump(const char *buffer, uint16_t size)
{
  // would rather heap allocate than use VLA
  std::vector< char > hex_buffer(size * 3 + 1);
  hex_buffer[size * 3] = 0;
  for(unsigned int j = 0; j < size; j++)
    sprintf(&hex_buffer[3 * j], "%02X ", buffer[j]);
  std::string str(hex_buffer.begin(), hex_buffer.end());
  llarp::LogInfo("First ", size, " bytes: ", str);
}

void
hexDumpAt(const char *const buffer, uint32_t pos, uint16_t size)
{
  // would rather heap allocate than use VLA
  std::vector< char > hex_buffer(size * 3 + 1);
  hex_buffer[size * 3] = 0;
  for(unsigned int j = 0; j < size; j++)
    sprintf(&hex_buffer[3 * j], "%02X ", buffer[pos + j]);
  std::string str(hex_buffer.begin(), hex_buffer.end());
  llarp::LogInfo(pos, " ", size, " bytes: ", str);
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
code_domain(char *&buffer, const std::string &domain) noexcept
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

void
vcode_domain(std::vector< byte_t > &bytes, const std::string &domain) noexcept
{
  std::string::size_type start(0);
  std::string::size_type end;  // indexes
  // llarp::LogInfo("domain [", domain, "]");
  while((end = domain.find('.', start)) != std::string::npos)
  {
    bytes.push_back(end - start);  // label length octet
    for(std::string::size_type i = start; i < end; i++)
    {
      bytes.push_back(domain[i]);  // label octets
      // llarp::LogInfo("Writing ", domain[i], " at ", i);
    }
    start = end + 1;  // Skip '.'
  }

  // llarp::LogInfo("start ", start, " domain size ", domain.size());

  bytes.push_back(domain.size() - start);  // last label length octet
  for(size_t i = start; i < domain.size(); i++)
  {
    bytes.push_back(domain[i]);  // last label octets
    // llarp::LogInfo("Writing ", domain[i], " at ", i);
  }
  bytes.push_back(0);  // end it
}

// expects host order
void
vput16bits(std::vector< byte_t > &bytes, uint16_t value) noexcept
{
  char buf[2]        = {0};
  char *write_buffer = buf;
  htobe16buf(write_buffer, value);
  bytes.push_back(buf[0]);
  bytes.push_back(buf[1]);
}

// expects host order
void
vput32bits(std::vector< byte_t > &bytes, uint32_t value) noexcept
{
  char buf[4]        = {0};
  char *write_buffer = buf;
  htobe32buf(write_buffer, value);
  bytes.push_back(buf[0]);
  bytes.push_back(buf[1]);
  bytes.push_back(buf[2]);
  bytes.push_back(buf[3]);
}

void
dns_writeType(std::vector< byte_t > &bytes, llarp::dns::record *record)
{
  auto *type1a = dynamic_cast< llarp::dns::type_1a * >(record);
  if(type1a)
  {
    std::vector< byte_t > more_bytes = type1a->to_bytes();
    llarp::LogDebug("[1]Adding ", more_bytes.size());
    bytes.insert(bytes.end(), more_bytes.begin(), more_bytes.end());
  }

  auto *type2ns = dynamic_cast< llarp::dns::type_2ns * >(record);
  if(type2ns)
  {
    std::vector< byte_t > more_bytes = type2ns->to_bytes();
    llarp::LogDebug("[2]Adding ", more_bytes.size());
    bytes.insert(bytes.end(), more_bytes.begin(), more_bytes.end());
  }

  auto *type5cname = dynamic_cast< llarp::dns::type_5cname * >(record);
  if(type5cname)
  {
    std::vector< byte_t > more_bytes = type5cname->to_bytes();
    llarp::LogDebug("[5]Adding ", more_bytes.size());
    bytes.insert(bytes.end(), more_bytes.begin(), more_bytes.end());
  }

  auto *type12ptr = dynamic_cast< llarp::dns::type_12ptr * >(record);
  if(type12ptr)
  {
    std::vector< byte_t > more_bytes = type12ptr->to_bytes();
    llarp::LogDebug("[12]Adding ", more_bytes.size());
    bytes.insert(bytes.end(), more_bytes.begin(), more_bytes.end());
  }
  auto *type15mx = dynamic_cast< llarp::dns::type_15mx * >(record);
  if(type15mx)
  {
    std::vector< byte_t > more_bytes = type15mx->to_bytes();
    llarp::LogDebug("[15]Adding ", more_bytes.size());
    bytes.insert(bytes.end(), more_bytes.begin(), more_bytes.end());
  }
  auto *type16txt = dynamic_cast< llarp::dns::type_16txt * >(record);
  if(type16txt)
  {
    std::vector< byte_t > more_bytes = type16txt->to_bytes();
    llarp::LogDebug("[15]Adding ", more_bytes.size());
    bytes.insert(bytes.end(), more_bytes.begin(), more_bytes.end());
  }
}

std::vector< byte_t >
packet2bytes(dns_packet &in)
{
  std::vector< byte_t > write_buffer;
  vput16bits(write_buffer, in.header.id);

  int fields = (in.header.qr << 15);   // QR => message type, 1 = response
  fields += (in.header.opcode << 14);  // I think opcode is always 0
  fields += in.header.rcode;           // response code (3 => not found, 0 = Ok)
  vput16bits(write_buffer, fields);

  // don't pull these from the header, trust what we actually have more
  vput16bits(write_buffer, in.questions.size());  // QD (number of questions)
  vput16bits(write_buffer, in.answers.size());    // AN (number of answers)
  vput16bits(write_buffer, in.auth_rrs.size());   // NS (number of auth RRs)
  vput16bits(write_buffer,
             in.additional_rrs.size());  // AR (number of Additional RRs)

  for(auto &it : in.questions)
  {
    // code question
    vcode_domain(write_buffer, it->name);
    vput16bits(write_buffer, it->type);
    vput16bits(write_buffer, it->qClass);
  }

  for(auto &it : in.answers)
  {
    // code answers
    vcode_domain(write_buffer, it->name);
    vput16bits(write_buffer, it->type);
    vput16bits(write_buffer, it->aClass);
    vput32bits(write_buffer, 1);  // ttl
    dns_writeType(write_buffer, it->record.get());
  }

  for(auto &it : in.auth_rrs)
  {
    // code answers
    vcode_domain(write_buffer, it->name);
    vput16bits(write_buffer, it->type);
    vput16bits(write_buffer, it->aClass);
    vput32bits(write_buffer, 1);  // ttl
    dns_writeType(write_buffer, it->record.get());
  }

  for(auto &it : in.additional_rrs)
  {
    // code answers
    vcode_domain(write_buffer, it->name);
    vput16bits(write_buffer, it->type);
    vput16bits(write_buffer, it->aClass);
    vput32bits(write_buffer, 1);  // ttl
    dns_writeType(write_buffer, it->record.get());
  }

  return write_buffer;
}

extern "C"
{
  uint16_t
  get16bits(const char *&buffer) noexcept
  {
    uint16_t value = bufbe16toh(buffer);
    buffer += 2;
    return value;
  }

  uint32_t
  get32bits(const char *&buffer) noexcept
  {
    uint32_t value = bufbe32toh(buffer);
    buffer += 4;
    return value;
  }

  bool
  decode_hdr(llarp_buffer_t *buffer, dns_msg_header *hdr)
  {
    uint16_t fields;

    // reads as HOST byte order
    if(!buffer->read_uint16(hdr->id))
      return false;
    if(!buffer->read_uint16(fields))
      return false;
    if(!buffer->read_uint16(hdr->qdCount))
      return false;
    if(!buffer->read_uint16(hdr->anCount))
      return false;
    if(!buffer->read_uint16(hdr->nsCount))
      return false;
    if(!buffer->read_uint16(hdr->arCount))
      return false;

    // decode fields into hdr
    uint8_t lFields = (fields & 0x00FF) >> 0;
    uint8_t hFields = (fields & 0xFF00) >> 8;

    // process high byte
    // hdr->qr      = fields & 0x8000;
    hdr->qr     = (hFields >> 7) & 0x1;
    hdr->opcode = fields & 0x7800;
    hdr->aa     = fields & 0x0400;
    hdr->tc     = fields & 0x0200;
    hdr->rd     = fields & 0x0100;

    // process low byte
    hdr->ra    = (lFields >> 7) & 0x1;
    hdr->z     = (lFields >> 6) & 0x1;
    hdr->ad    = (lFields >> 5) & 0x1;
    hdr->cd    = (lFields >> 4) & 0x1;
    hdr->rcode = lFields & 0xf;
    return true;
  }

  dns_msg_question *
  decode_question(const char *buffer, uint32_t *pos)
  {
    auto *question = new dns_msg_question;

    std::string m_qName = getDNSstring(buffer, pos);
    llarp::LogDebug("Got question name: ", m_qName);

    const char *moveable = buffer;
    moveable += *pos;  // advance to position
    question->name = m_qName;

    question->type = get16bits(moveable);
    (*pos) += 2;
    question->qClass = get16bits(moveable);
    (*pos) += 2;
    return question;
  }

  dns_msg_answer *
  decode_answer(const char *const buffer, uint32_t *pos)
  {
    auto *answer = new dns_msg_answer;
    /*
    llarp_buffer_t bob;
    bob.base = (unsigned char *)buffer;
    bob.sz = 12;
    llarp::DumpBuffer(bob);
    */

    const char *moveable = buffer;
    // llarp::LogDebug("Advancing to pos ", std::to_string(*pos));
    moveable += (*pos);  // advance to position

    // hexDump(moveable, 12);
    // hexDumpAt(buffer, *pos, 12);

    if(*moveable == '\xc0')
    {
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
        llarp::LogInfo("Parsed string ", answer->name, " read ",
      std::to_string(readAt32)); moveable += readAt32; (*pos) += readAt32;
      */
      // moveable++; (*pos)++;
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

      answer->record = std::make_unique< llarp::dns::type_1a >();
      answer->record->parse(answer->rData);
    }
    else
    {
      llarp::LogDebug("Got type ", answer->type);
      // FIXME: move this out of here, this shouldn't be responsible for decode
      switch(answer->type)
      {
        case LLARP_DNS_RECTYPE_NS:  // NS
        {
          std::string ns = getDNSstring(buffer, pos);
          answer->rData.resize(ns.size());
          memcpy(answer->rData.data(), ns.c_str(),
                 ns.size());  // raw copy rData

          // don't really need to do anything here
          moveable += answer->rdLen;
          //(*pos) += answer->rdLen;  // advance the length

          answer->record = std::make_unique< llarp::dns::type_2ns >();
          answer->record->parse(answer->rData);
        }
        break;
        case LLARP_DNS_RECTYPE_CNAME:  // CNAME
        {
          std::string cname = getDNSstring(buffer, pos);
          llarp::LogDebug("CNAME ", cname);
          answer->rData.resize(cname.size());
          memcpy(answer->rData.data(), cname.c_str(), cname.size());

          moveable += answer->rdLen;
          //(*pos) += answer->rdLen;  // advance the length

          answer->record = std::make_unique< llarp::dns::type_5cname >();
          answer->record->parse(answer->rData);
        }
        break;
        case LLARP_DNS_RECTYPE_SOA:  // type 6 = SOA
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
        case LLARP_DNS_RECTYPE_PTR:
        {
          std::string revname = getDNSstring(buffer, pos);
          llarp::LogInfo("revDNSname: ", revname);
          // answer->rData = new uint8_t[answer->rdLen + 1];
          answer->rData.resize(revname.size());
          memcpy(answer->rData.data(), revname.c_str(), revname.size());
          // answer->rData = (uint8_t *)strdup(revname.c_str()); // safer? nope
          moveable += answer->rdLen;
          //(*pos) += answer->rdLen;  // advance the length

          answer->record = std::make_unique< llarp::dns::type_12ptr >();
          answer->record->parse(answer->rData);
        }
        break;
        case LLARP_DNS_RECTYPE_MX:
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

          answer->record = std::make_unique< llarp::dns::type_15mx >();
          answer->record->parse(answer->rData);
        }
        break;
        case LLARP_DNS_RECTYPE_TXT:
        {
          // hexDump(buffer, 5);
          // std::string revname = getDNSstring((char *)buffer);
          llarp::LogInfo("TXT: size: ", answer->rdLen);
          answer->rData.resize(answer->rdLen);
          memcpy(answer->rData.data(), moveable + 1, answer->rdLen);
          moveable += answer->rdLen;
          (*pos) += answer->rdLen;  // advance the length

          answer->record = std::make_unique< llarp::dns::type_16txt >();
          answer->record->parse(answer->rData);
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
  put16bits(char *&buffer, uint16_t value) noexcept
  {
    htobe16buf(buffer, value);
    buffer += 2;
  }

  void
  put32bits(char *&buffer, uint32_t value) noexcept
  {
    htobe32buf(buffer, value);
    buffer += 4;
  }

  void
  llarp_handle_dns_recvfrom(struct llarp_udp_io *udp,
                            const struct sockaddr *addr, ManagedBuffer buf)
  {
    // auto buffer = llarp::StackBuffer< decltype(castBuf) >(castBuf);
    dns_msg_header hdr;
    if(!decode_hdr(&buf.underlying, &hdr))
    {
      llarp::LogError("failed to decode dns header");
      return;
    }
    // rewind
    buf.underlying.cur = buf.underlying.base;
    llarp::LogDebug("msg id ", hdr.id);
    llarp::LogDebug("msg qr ", (uint8_t)hdr.qr);
    if(!udp)
    {
      llarp::LogError("no udp passed in to handler");
    }
    if(!addr)
    {
      llarp::LogError("no source addr passed in to handler");
    }
    if(hdr.qr)
    {
      llarp::LogDebug("handling as dnsc answer");
      llarp_handle_dnsc_recvfrom(udp, addr, buf);
    }
    else
    {
      llarp::LogDebug("handling as dnsd question");
      llarp_handle_dnsd_recvfrom(udp, addr, buf);
    }
  }
}
