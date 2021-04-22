#include "ip_range.hpp"

#include "oxenmq/bt_serialize.h"

#include "llarp/util/bencode.h"

namespace llarp
{
  bool
  IPRange::BEncode(llarp_buffer_t* buf) const
  {
    const auto str = oxenmq::bt_serialize(ToString());
    return buf->write(str.begin(), str.end());
  }

  bool
  IPRange::BDecode(llarp_buffer_t* buf)
  {
    const auto* start = buf->cur;
    if (not bencode_discard(buf))
      return false;
    std::string_view data{
        reinterpret_cast<const char*>(start), static_cast<size_t>(buf->cur - start)};
    std::string str;
    try
    {
      oxenmq::bt_deserialize(data, str);
    }
    catch (std::exception&)
    {
      return false;
    }
    return FromString(str);
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
        const auto bits = stoi(bitsstr);
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
  IPRange::BaseAddressString() const
  {
    if (IsV4())
    {
      const huint32_t addr4 = net::TruncateV6(addr);
      return addr4.ToString();
    }
    return addr.ToString();
  }

}  // namespace llarp
