#ifndef LLARP_XI_HPP
#define LLARP_XI_HPP
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/net.hpp>
#include <iostream>
#include <llarp/bits.hpp>

/**
 * exit_info.h
 *
 * utilities for handling exits on the llarp network
 */

/// Exit info model
namespace llarp
{
  struct ExitInfo final : public IBEncodeMessage
  {
    struct in6_addr address;
    struct in6_addr netmask;
    PubKey pubkey;

    ExitInfo(const PubKey &pk, const nuint32_t &ipv4_exit) : IBEncodeMessage()
    {
      pubkey = pk;
      memset(address.s6_addr, 0, 16);
      address.s6_addr[11] = 0xff;
      address.s6_addr[10] = 0xff;
      memcpy(address.s6_addr + 12, &ipv4_exit, 4);
      memset(netmask.s6_addr, 0xff, 16);
    }

    ExitInfo() : IBEncodeMessage()
    {
    }

    ExitInfo(const ExitInfo &other) : IBEncodeMessage()
    {
      pubkey = other.pubkey;
      memcpy(address.s6_addr, other.address.s6_addr, 16);
      memcpy(netmask.s6_addr, other.netmask.s6_addr, 16);
      version = other.version;
    }

    ~ExitInfo();

    bool
    BEncode(llarp_buffer_t *buf) const override;

    bool
    DecodeKey(llarp_buffer_t k, llarp_buffer_t *buf) override;

    ExitInfo &
    operator=(const ExitInfo &other);

    friend std::ostream &
    operator<<(std::ostream &out, const ExitInfo &xi)
    {
      char tmp[128] = {0};
      if(inet_ntop(AF_INET6, (void *)&xi.address, tmp, sizeof(tmp)))
        out << std::string(tmp);
      else
        return out;
      out << std::string("/");
#if defined(ANDROID) || defined(RPI)
      snprintf(tmp, sizeof(tmp), "%lu",
               llarp::bits::count_array_bits(xi.netmask.s6_addr));
      return out << tmp;
#else
      return out << std::to_string(
                 llarp::bits::count_array_bits(xi.netmask.s6_addr));
#endif
    }
  };
}  // namespace llarp

#endif
