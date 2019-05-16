#include <net/net_int.hpp>

#include <string>

namespace llarp
{
  template <>
  void
  huint32_t::ToV6(V6Container& c)
  {
    c.resize(16);
    std::fill(c.begin(), c.end(), 0);
    htobe32buf(c.data() + 12, h);
    c[11] = 0xff;
    c[10] = 0xff;
  }

  template <>
  std::string
  huint32_t::ToString() const
  {
    uint32_t n                = htonl(h);
    char tmp[INET_ADDRSTRLEN] = {0};
    if(!inet_ntop(AF_INET, (void*)&n, tmp, sizeof(tmp)))
      return "";
    return tmp;
  }
  template <>
  std::string
  nuint32_t::ToString() const
  {
    char tmp[INET_ADDRSTRLEN] = {0};
    if(!inet_ntop(AF_INET, (void*)&n, tmp, sizeof(tmp)))
      return "";
    return tmp;
  }
  template <>
  std::string
  huint16_t::ToString() const
  {
    return std::to_string(h);
  }

  template <>
  std::string
  nuint16_t::ToString() const
  {
    return std::to_string(ntohs(n));
  }
}  // namespace llarp