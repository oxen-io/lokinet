#ifndef _WIN32
#include <arpa/inet.h>
#endif

#include <net/exit_info.hpp>
#include <util/bencode.h>
#include <util/bits.hpp>
#include <util/mem.h>

#include <list>
#include <cstring>

namespace llarp
{
  bool
  ExitInfo::BEncode(llarp_buffer_t* buf) const
  {
    char tmp[128] = {0};
    if (!bencode_start_dict(buf))
      return false;

    if (!inet_ntop(AF_INET6, address.s6_addr, tmp, sizeof(tmp)))
      return false;
    if (!BEncodeWriteDictString("a", std::string(tmp), buf))
      return false;

    if (!inet_ntop(AF_INET6, netmask.s6_addr, tmp, sizeof(tmp)))
      return false;
    if (!BEncodeWriteDictString("b", std::string(tmp), buf))
      return false;

    if (!BEncodeWriteDictEntry("k", pubkey, buf))
      return false;

    if (!BEncodeWriteDictInt("v", version, buf))
      return false;

    return bencode_end(buf);
  }

  static bool
  bdecode_ip_string(llarp_buffer_t* buf, in6_addr& ip)
  {
    char tmp[128] = {0};
    llarp_buffer_t strbuf;
    if (!bencode_read_string(buf, &strbuf))
      return false;

    if (strbuf.sz >= sizeof(tmp))
      return false;

    memcpy(tmp, strbuf.base, strbuf.sz);
    tmp[strbuf.sz] = 0;
    return inet_pton(AF_INET6, tmp, &ip.s6_addr[0]) == 1;
  }

  bool
  ExitInfo::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictEntry("k", pubkey, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("v", version, read, k, buf))
      return false;
    if (k == "a")
      return bdecode_ip_string(buf, address);
    if (k == "b")
      return bdecode_ip_string(buf, netmask);
    return read;
  }

  std::ostream&
  ExitInfo::print(std::ostream& stream, int level, int spaces) const
  {
    Printer printer(stream, level, spaces);

    std::ostringstream ss;
    char tmp[128] = {0};

    if (inet_ntop(AF_INET6, (void*)&address, tmp, sizeof(tmp)))
      ss << tmp;
    else
      return stream;
    ss << std::string("/");
#if defined(ANDROID) || defined(RPI)
    snprintf(tmp, sizeof(tmp), "%zu", llarp::bits::count_array_bits(netmask.s6_addr));
    ss << tmp;
#else
    ss << std::to_string(llarp::bits::count_array_bits(netmask.s6_addr));
#endif
    printer.printValue(ss.str());
    return stream;
  }

}  // namespace llarp
