#ifndef LLARP_AI_HPP
#define LLARP_AI_HPP
#include <llarp/mem.h>
#include <llarp/net.h>
#include <stdbool.h>
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
  struct AddressInfo
  {
    uint16_t rank;
    std::string dialect;
    llarp::PubKey pubkey;
    struct in6_addr ip;
    uint16_t port;

    bool
    BEncode(llarp_buffer_t *buf) const;

    bool
    BDecode(llarp_buffer_t *buf);
  };

}  // namespace llarp

#endif
