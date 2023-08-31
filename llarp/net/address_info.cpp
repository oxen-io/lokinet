#include "address_info.hpp"
#include <stdexcept>

#ifndef _WIN32
#include <arpa/inet.h>
#endif
#include "net.hpp"
#include <llarp/util/bencode.h>
#include <llarp/util/mem.h>

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
    return std::tie(lhs.rank, lhs.ip, lhs.port) < std::tie(rhs.rank, rhs.ip, rhs.port);
  }

  std::variant<nuint32_t, nuint128_t>
  AddressInfo::IP() const
  {
    return SockAddr{ip}.getIP();
  }

  bool
  AddressInfo::decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    uint64_t i;
    char tmp[128] = {0};

    llarp_buffer_t strbuf;

    // rank
    if (key.startswith("c"))
    {
      if (!bencode_read_integer(buf, &i))
        return false;

      if (i > 65536 || i <= 0)
        return false;

      rank = i;
      return true;
    }

    // dialect
    if (key.startswith("d"))
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
    if (key.startswith("e"))
    {
      return pubkey.BDecode(buf);
    }

    // ip address
    if (key.startswith("i"))
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
    if (key.startswith("p"))
    {
      if (!bencode_read_integer(buf, &i))
        return false;

      if (i > 65536 || i <= 0)
        return false;

      port = i;
      return true;
    }

    // version
    if (key.startswith("v"))
    {
      if (!bencode_read_integer(buf, &i))
        return false;
      return i == llarp::constants::proto_version;
    }

    // bad key
    return false;
  }

  std::string
  AddressInfo::bt_encode() const
  {
    char ipbuff[128] = {0};
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("c", rank);
      btdp.append("d", dialect);
      btdp.append("e", pubkey.ToView());

      const char* ipstr = inet_ntop(AF_INET6, (void*)&ip, ipbuff, sizeof(ipbuff));

      btdp.append("i", std::string_view{ipstr, strnlen(ipstr, sizeof(ipbuff))});
      btdp.append("p", port);
      btdp.append("v", version);
    }
    catch (...)
    {
      log::critical(net_cat, "Error: AddressInfo failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  IpAddress
  AddressInfo::toIpAddress() const
  {
    SockAddr addr(ip);
    addr.setPort(port);
    return {addr};
  }

  void
  AddressInfo::fromSockAddr(const SockAddr& addr)
  {
    const auto* addr6 = static_cast<const sockaddr_in6*>(addr);
    memcpy(ip.s6_addr, addr6->sin6_addr.s6_addr, sizeof(ip.s6_addr));
    port = addr.getPort();
  }

  std::string
  AddressInfo::ToString() const
  {
    char tmp[INET6_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET6, (void*)&ip, tmp, sizeof(tmp));
    return fmt::format("[{}]:{}", tmp, port);
  }

  std::string
  AddressInfo::IPString() const
  {
    char tmp[INET6_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET6, (void*)&ip, tmp, sizeof(tmp));
    return std::string{tmp};
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
