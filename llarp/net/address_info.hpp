#ifndef LLARP_AI_HPP
#define LLARP_AI_HPP

#include <crypto/types.hpp>
#include <net/net.h>
#include <util/bencode.hpp>
#include <util/mem.h>

#include <string>
#include <vector>
#include <stdbool.h>

/**
 * address_info.hpp
 *
 * utilities for handling addresses on the llarp network
 */

/// address information model
namespace llarp
{
  struct AddressInfo final : public IBEncodeMessage
  {
    uint16_t rank;
    std::string dialect;
    llarp::PubKey pubkey;
    struct in6_addr ip;
    uint16_t port;

    AddressInfo() : IBEncodeMessage()
    {
    }

    AddressInfo(const AddressInfo& other)
        : IBEncodeMessage(other.version)
        , rank(other.rank)
        , dialect(other.dialect)
        , pubkey(other.pubkey)
        , port(other.port)
    {
      memcpy(ip.s6_addr, other.ip.s6_addr, 16);
    }

    ~AddressInfo();

    AddressInfo&
    operator=(const AddressInfo& other);

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf) override;

    std::ostream&
    print(std::ostream& stream, int level, int spaces) const;

    struct Hash
    {
      size_t
      operator()(const AddressInfo& addr) const
      {
        return AlignedBuffer< PUBKEYSIZE >::Hash()(addr.pubkey);
      }
    };
  };

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

#endif
