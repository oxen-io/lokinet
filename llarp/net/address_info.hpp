#pragma once

#include <llarp/crypto/types.hpp>
#include "ip_address.hpp"
#include "net.h"
#include <llarp/util/bencode.hpp>
#include <llarp/util/mem.h>

#include <string>
#include <vector>

/**
 * address_info.hpp
 *
 * utilities for handling addresses on the llarp network
 */

/// address information model
namespace llarp
{
  struct AddressInfo
  {
    uint16_t rank;
    std::string dialect;
    llarp::PubKey pubkey;
    in6_addr ip = {};
    uint16_t port;
    uint64_t version = LLARP_PROTO_VERSION;

    bool
    BDecode(llarp_buffer_t* buf)
    {
      return bencode_decode_dict(*this, buf);
    }

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf);

    /// Return an IpAddress representing the address portion of this AddressInfo
    IpAddress
    toIpAddress() const;

    /// Updates our ip and port to reflect that of the given SockAddr
    void
    fromSockAddr(const SockAddr& address);

    std::ostream&
    print(std::ostream& stream, int level, int spaces) const;
  };

  void
  to_json(nlohmann::json& j, const AddressInfo& a);

  inline std::ostream&
  operator<<(std::ostream& out, const AddressInfo& a)
  {
    return a.print(out, -1, -1);
  }

  bool
  operator==(const AddressInfo& lhs, const AddressInfo& rhs);

  bool
  operator<(const AddressInfo& lhs, const AddressInfo& rhs);

}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::AddressInfo>
  {
    size_t
    operator()(const llarp::AddressInfo& addr) const
    {
      return std::hash<llarp::PubKey>{}(addr.pubkey);
    }
  };
}  // namespace std
