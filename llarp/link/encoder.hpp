#ifndef LLARP_LINK_ENCODER_HPP
#define LLARP_LINK_ENCODER_HPP

#include <router_contact.hpp>

#include <util/bencode.h>
#include <util/buffer.h>

namespace llarp
{
  /// encode Link Introduce Message onto a buffer
  /// if router is nullptr then the LIM's r member is omitted.
  bool
  EncodeLIM(llarp_buffer_t* buff, const RouterContact* router,
            const KeyExchangeNonce& n);
}  // namespace llarp

#endif
