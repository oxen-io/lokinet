#ifndef LLARP_XI_HPP
#define LLARP_XI_HPP
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/net.h>
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
      if(inet_ntop(AF_INET6, &xi.address, tmp, sizeof(tmp)))
        out << std::string(tmp);
      else
        return out;
      out << std::string("/");
      return out << std::to_string(
                 llarp::bits::count_array_bits(xi.netmask.s6_addr));
    }
  };
}  // namespace llarp

#endif
