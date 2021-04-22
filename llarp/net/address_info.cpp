#include "address_info.hpp"
#include <stdexcept>

#ifndef _WIN32
#include <arpa/inet.h>
#endif
#include "net.hpp"
#include <llarp/util/bencode.h>
#include <llarp/util/mem.h>
#include <llarp/util/printer.hpp>

#include <cstring>

namespace llarp
{
  bool
  operator==(const AddressInfo& lhs, const AddressInfo& rhs)
  {
    // we don't care about rank
    return lhs.pubkey == rhs.pubkey && lhs.port == rhs.port && lhs.dialect == rhs.dialect
        && lhs.ip == rhs.ip;
  }

  bool
  operator<(const AddressInfo& lhs, const AddressInfo& rhs)
  {
    return lhs.rank < rhs.rank || lhs.ip < rhs.ip || lhs.port < rhs.port;
  }

  bool
  AddressInfo::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    uint64_t i;
    char tmp[128] = {0};

    llarp_buffer_t strbuf;

    // rank
    if (key == "c")
    {
      if (!bencode_read_integer(buf, &i))
        return false;

      if (i > 65536 || i <= 0)
        return false;

      rank = i;
      return true;
    }

    // dialect
    if (key == "d")
    {
      if (!bencode_read_string(buf, &strbuf))
        return false;
      if (strbuf.sz > sizeof(tmp))
        return false;
      memcpy(tmp, strbuf.base, strbuf.sz);
      tmp[strbuf.sz] = 0;
      dialect = std::string(tmp);
      return true;
    }

    // encryption public key
    if (key == "e")
    {
      return pubkey.BDecode(buf);
    }

    // ip address
    if (key == "i")
    {
      if (!bencode_read_string(buf, &strbuf))
        return false;

      if (strbuf.sz >= sizeof(tmp))
        return false;

      memcpy(tmp, strbuf.base, strbuf.sz);
      tmp[strbuf.sz] = 0;
      return inet_pton(AF_INET6, tmp, &ip.s6_addr[0]) == 1;
    }

    // port
    if (key == "p")
    {
      if (!bencode_read_integer(buf, &i))
        return false;

      if (i > 65536 || i <= 0)
        return false;

      port = i;
      return true;
    }

    // version
    if (key == "v")
    {
      if (!bencode_read_integer(buf, &i))
        return false;
      return i == LLARP_PROTO_VERSION;
    }

    // bad key
    return false;
  }

  bool
  AddressInfo::BEncode(llarp_buffer_t* buff) const
  {
    char ipbuff[128] = {0};
    const char* ipstr;
    if (!bencode_start_dict(buff))
      return false;
    /* rank */
    if (!bencode_write_bytestring(buff, "c", 1))
      return false;
    if (!bencode_write_uint64(buff, rank))
      return false;
    /* dialect */
    if (!bencode_write_bytestring(buff, "d", 1))
      return false;
    if (!bencode_write_bytestring(buff, dialect.c_str(), dialect.size()))
      return false;
    /* encryption key */
    if (!bencode_write_bytestring(buff, "e", 1))
      return false;
    if (!bencode_write_bytestring(buff, pubkey.data(), PUBKEYSIZE))
      return false;
    /** ip */
    ipstr = inet_ntop(AF_INET6, (void*)&ip, ipbuff, sizeof(ipbuff));
    if (!ipstr)
      return false;
    if (!bencode_write_bytestring(buff, "i", 1))
      return false;
    if (!bencode_write_bytestring(buff, ipstr, strnlen(ipstr, sizeof(ipbuff))))
      return false;
    /** port */
    if (!bencode_write_bytestring(buff, "p", 1))
      return false;
    if (!bencode_write_uint64(buff, port))
      return false;

    /** version */
    if (!bencode_write_uint64_entry(buff, "v", 1, LLARP_PROTO_VERSION))
      return false;
    /** end */
    return bencode_end(buff);
  }

  IpAddress
  AddressInfo::toIpAddress() const
  {
    SockAddr addr(ip);
    addr.setPort(port);
    return IpAddress(addr);
  }

  void
  AddressInfo::fromSockAddr(const SockAddr& addr)
  {
    const sockaddr_in6* addr6 = addr;
    memcpy(ip.s6_addr, addr6->sin6_addr.s6_addr, sizeof(ip.s6_addr));
    port = addr.getPort();
  }

  std::ostream&
  AddressInfo::print(std::ostream& stream, int level, int spaces) const
  {
    char tmp[128] = {0};
    inet_ntop(AF_INET6, (void*)&ip, tmp, sizeof(tmp));

    Printer printer(stream, level, spaces);
    printer.printAttribute("ip", tmp);
    printer.printAttribute("port", port);

    return stream;
  }

  void
  to_json(nlohmann::json& j, const AddressInfo& a)
  {
    char tmp[128] = {0};
    inet_ntop(AF_INET6, (void*)&a.ip, tmp, sizeof(tmp));

    j = nlohmann::json{
        {"rank", a.rank},
        {"dialect", a.dialect},
        {"pubkey", a.pubkey.ToString()},
        {"in6_addr", tmp},
        {"port", a.port}};
  }
}  // namespace llarp
