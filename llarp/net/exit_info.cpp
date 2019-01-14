#ifndef _WIN32
#include <arpa/inet.h>
#endif

#include <net/exit_info.hpp>
#include <util/bencode.h>
#include <util/mem.h>

#include <list>
#include <string.h>

namespace llarp
{
  ExitInfo::~ExitInfo()
  {
  }

  ExitInfo&
  ExitInfo::operator=(const ExitInfo& other)
  {
    memcpy(address.s6_addr, other.address.s6_addr, 16);
    memcpy(netmask.s6_addr, other.netmask.s6_addr, 16);
    pubkey  = other.pubkey;
    version = other.version;
    return *this;
  }

  bool
  ExitInfo::BEncode(llarp_buffer_t* buf) const
  {
    char tmp[128] = {0};
    if(!bencode_start_dict(buf))
      return false;

    if(!inet_ntop(AF_INET6, (void*)&address, tmp, sizeof(tmp)))
      return false;
    if(!BEncodeWriteDictString("a", std::string(tmp), buf))
      return false;

    if(!inet_ntop(AF_INET6, (void*)&netmask, tmp, sizeof(tmp)))
      return false;
    if(!BEncodeWriteDictString("b", std::string(tmp), buf))
      return false;

    if(!BEncodeWriteDictEntry("k", pubkey, buf))
      return false;

    if(!BEncodeWriteDictInt("v", version, buf))
      return false;

    return bencode_end(buf);
  }

  bool
  ExitInfo::DecodeKey(__attribute__((unused)) llarp_buffer_t k,
                      __attribute__((unused)) llarp_buffer_t* buf)
  {
    bool read = false;
    // TODO: implement me
    return read;
  }

}  // namespace llarp
