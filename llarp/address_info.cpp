#include <llarp/address_info.hpp>
#ifndef _WIN32
#include <arpa/inet.h>
#endif
#include <llarp/bencode.h>
#include <llarp/mem.h>
#include <llarp/string.h>
#include <llarp/net.hpp>

namespace llarp
{
  AddressInfo::~AddressInfo()
  {
  }

  AddressInfo &
  AddressInfo::operator=(const AddressInfo &other)
  {
    rank    = other.rank;
    dialect = other.dialect;
    pubkey  = other.pubkey;
    memcpy(ip.s6_addr, other.ip.s6_addr, 16);
    port = other.port;
    return *this;
  }

  bool
  AddressInfo::operator==(const AddressInfo &other) const
  {
    // we don't care about rank
    return pubkey == other.pubkey && port == other.port
        && dialect == other.dialect && ip == other.ip;
  }

  bool
  AddressInfo::operator<(const AddressInfo &other) const
  {
    return rank < other.rank || ip < other.ip || port < other.port;
  }

  bool
  AddressInfo::DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf)
  {
    uint64_t i;
    char tmp[128] = {0};

    llarp_buffer_t strbuf;

    // rank
    if(llarp_buffer_eq(key, "c"))
    {
      if(!bencode_read_integer(buf, &i))
        return false;

      if(i > 65536 || i <= 0)
        return false;

      rank = i;
      return true;
    }

    // dialect
    if(llarp_buffer_eq(key, "d"))
    {
      if(!bencode_read_string(buf, &strbuf))
        return false;
      if(strbuf.sz > sizeof(tmp))
        return false;
      memcpy(tmp, strbuf.base, strbuf.sz);
      tmp[strbuf.sz] = 0;
      dialect        = std::string(tmp);
      return true;
    }

    // encryption public key
    if(llarp_buffer_eq(key, "e"))
    {
      return pubkey.BDecode(buf);
    }

    // ip address
    if(llarp_buffer_eq(key, "i"))
    {
      if(!bencode_read_string(buf, &strbuf))
        return false;

      if(strbuf.sz >= sizeof(tmp))
        return false;

      memcpy(tmp, strbuf.base, strbuf.sz);
      tmp[strbuf.sz] = 0;
      return inet_pton(AF_INET6, tmp, &ip.s6_addr[0]) == 1;
    }

    // port
    if(llarp_buffer_eq(key, "p"))
    {
      if(!bencode_read_integer(buf, &i))
        return false;

      if(i > 65536 || i <= 0)
        return false;

      port = i;
      return true;
    }

    // version
    if(llarp_buffer_eq(key, "v"))
    {
      if(!bencode_read_integer(buf, &i))
        return false;
      return i == LLARP_PROTO_VERSION;
    }

    // bad key
    return false;
  }

  bool
  AddressInfo::BEncode(llarp_buffer_t *buff) const
  {
    char ipbuff[128] = {0};
    const char *ipstr;
    if(!bencode_start_dict(buff))
      return false;
    /* rank */
    if(!bencode_write_bytestring(buff, "c", 1))
      return false;
    if(!bencode_write_uint64(buff, rank))
      return false;
    /* dialect */
    if(!bencode_write_bytestring(buff, "d", 1))
      return false;
    if(!bencode_write_bytestring(buff, dialect.c_str(), dialect.size()))
      return false;
    /* encryption key */
    if(!bencode_write_bytestring(buff, "e", 1))
      return false;
    if(!bencode_write_bytestring(buff, pubkey, PUBKEYSIZE))
      return false;
    /** ip */
    ipstr = inet_ntop(AF_INET6, &ip, ipbuff, sizeof(ipbuff));
    if(!ipstr)
      return false;
    if(!bencode_write_bytestring(buff, "i", 1))
      return false;
    if(!bencode_write_bytestring(buff, ipstr, strnlen(ipstr, sizeof(ipbuff))))
      return false;
    /** port */
    if(!bencode_write_bytestring(buff, "p", 1))
      return false;
    if(!bencode_write_uint64(buff, port))
      return false;

    /** version */
    if(!bencode_write_version_entry(buff))
      return false;
    /** end */
    return bencode_end(buff);
  }
}  // namespace llarp
