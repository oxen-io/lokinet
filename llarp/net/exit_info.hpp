#pragma once

#include <llarp/crypto/types.hpp>
#include "ip_address.hpp"
#include <llarp/util/bencode.hpp>

#include <iosfwd>

/**
 * exit_info.h
 *
 * utilities for handling exits on the llarp network
 */

/// Exit info model
namespace llarp
{
  /// deprecated don't use me , this is only for backwards compat
  struct ExitInfo
  {
    IpAddress ipAddress;
    IpAddress netmask;
    PubKey pubkey;
    uint64_t version = LLARP_PROTO_VERSION;

    ExitInfo() = default;

    ExitInfo(const PubKey& pk, const IpAddress& address) : ipAddress(address), pubkey(pk)
    {}

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BDecode(llarp_buffer_t* buf)
    {
      return bencode_decode_dict(*this, buf);
    }

    bool
    DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf);

    std::ostream&
    print(std::ostream& stream, int level, int spaces) const;
  };

  inline std::ostream&
  operator<<(std::ostream& out, const ExitInfo& xi)
  {
    return xi.print(out, -1, -1);
  }
}  // namespace llarp
