#ifndef LLARP_XI_HPP
#define LLARP_XI_HPP

#include <crypto/types.hpp>
#include <net/net.hpp>
#include <util/bencode.hpp>

#include <iosfwd>

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

    ExitInfo(const PubKey &pk, const nuint32_t &ipv4_exit)
        : IBEncodeMessage(), pubkey(pk)
    {
      memset(address.s6_addr, 0, 16);
      address.s6_addr[11] = 0xff;
      address.s6_addr[10] = 0xff;
      memcpy(address.s6_addr + 12, &ipv4_exit, 4);
      memset(netmask.s6_addr, 0xff, 16);
    }

    ExitInfo() : IBEncodeMessage()
    {
    }

    ExitInfo(const ExitInfo &other)
        : IBEncodeMessage(other.version), pubkey(other.pubkey)
    {
      memcpy(address.s6_addr, other.address.s6_addr, 16);
      memcpy(netmask.s6_addr, other.netmask.s6_addr, 16);
    }

    ~ExitInfo();

    bool
    BEncode(llarp_buffer_t *buf) const override;

    bool
    DecodeKey(const llarp_buffer_t &k, llarp_buffer_t *buf) override;

    ExitInfo &
    operator=(const ExitInfo &other);

    std::ostream &
    print(std::ostream &stream, int level, int spaces) const;
  };

  inline std::ostream &
  operator<<(std::ostream &out, const ExitInfo &xi)
  {
    return xi.print(out, -1, -1);
  }
}  // namespace llarp

#endif
