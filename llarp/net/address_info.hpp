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

    friend std::ostream&
    operator<<(std::ostream& out, const AddressInfo& a)
    {
      char tmp[128] = {0};
      inet_ntop(AF_INET6, (void*)&a.ip, tmp, sizeof(tmp));
      out << tmp << ".";
#if defined(ANDROID) || defined(RPI)
      snprintf(tmp, sizeof(tmp), "%u", a.port);
      return out << tmp;
#else
      return out << std::to_string(a.port);
#endif
    }

    struct Hash
    {
      size_t
      operator()(const AddressInfo& addr) const
      {
        return AlignedBuffer< PUBKEYSIZE >::Hash()(addr.pubkey);
      }
    };
  };

  bool
  operator==(const AddressInfo& lhs, const AddressInfo& rhs);

  bool
  operator<(const AddressInfo& lhs, const AddressInfo& rhs);

}  // namespace llarp

#endif
