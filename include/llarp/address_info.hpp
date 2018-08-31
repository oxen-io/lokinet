#ifndef LLARP_AI_HPP
#define LLARP_AI_HPP
#include <llarp/mem.h>
#include <llarp/net.h>
#include <stdbool.h>
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>

#include <string>

/**
 * address_info.hpp
 *
 * utilities for handling addresses on the llarp network
 */

/// address information model
namespace llarp
{
  struct AddressInfo : public IBEncodeMessage
  {
    uint16_t rank;
    std::string dialect;
    llarp::PubKey pubkey;
    struct in6_addr ip;
    uint16_t port;

    ~AddressInfo();

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf);

    friend std::ostream&
    operator<<(std::ostream& out, const AddressInfo& a)
    {
      char tmp[128] = {0};
      inet_ntop(AF_INET6, &a.ip, tmp, sizeof(tmp));
      return out << tmp << "." << std::to_string(a.port);
    }
  };

}  // namespace llarp

#endif
