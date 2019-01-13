#ifndef LLARP_LINK_ENCODER_HPP
#define LLARP_LINK_ENCODER_HPP

struct llarp_buffer_t;

namespace llarp
{
  struct KeyExchangeNonce;
  class RouterContact;

  /// encode Link Introduce Message onto a buffer
  /// if router is nullptr then the LIM's r member is omitted.
  bool
  EncodeLIM(llarp_buffer_t* buff, const RouterContact* router,
            const KeyExchangeNonce& n);
}  // namespace llarp

#endif
