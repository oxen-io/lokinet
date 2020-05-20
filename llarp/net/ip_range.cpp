#include <net/ip_range.hpp>

namespace llarp
{
  bool
  IPRange::ContainsV4(const huint32_t& ip) const
  {
    return Contains(net::ExpandV4(ip));
  }

  bool
  IPRange::FromString(std::string str)
  {
    const auto colinpos = str.find(":");
    const auto slashpos = str.find("/");
    std::string bitsstr;
    if (slashpos != std::string::npos)
    {
      bitsstr = str.substr(slashpos + 1);
      str = str.substr(0, slashpos);
    }
    if (colinpos == std::string::npos)
    {
      huint32_t ip;
      if (!ip.FromString(str))
        return false;
      addr = net::ExpandV4(ip);
      if (!bitsstr.empty())
      {
        auto bits = atoi(bitsstr.c_str());
        if (bits < 0 || bits > 32)
          return false;
        netmask_bits = netmask_ipv6_bits(96 + bits);
      }
      else
        netmask_bits = netmask_ipv6_bits(128);
    }
    else
    {
      if (!addr.FromString(str))
        return false;
      if (!bitsstr.empty())
      {
        auto bits = atoi(bitsstr.c_str());
        if (bits < 0 || bits > 128)
          return false;
        netmask_bits = netmask_ipv6_bits(bits);
      }
      else
      {
        netmask_bits = netmask_ipv6_bits(128);
      }
    }
    return true;
  }

  std::string
  IPRange::ToString() const
  {
    char buf[INET6_ADDRSTRLEN + 1] = {0};
    std::string str;
    in6_addr inaddr = {};
    size_t numset = 0;
    uint128_t bits = netmask_bits.h;
    while (bits)
    {
      if (bits & 1)
        numset++;
      bits >>= 1;
    }
    str += inet_ntop(AF_INET6, &inaddr, buf, sizeof(buf));
    return str + "/" + std::to_string(numset);
  }

}  // namespace llarp
