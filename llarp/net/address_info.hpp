#ifndef LLARP_AI_HPP
#define LLARP_AI_HPP

#include <crypto/types.hpp>
#include <net/net.h>
#include <util/bencode.hpp>
#include <util/mem.h>

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
    in6_addr ip = {0};
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

#endif
