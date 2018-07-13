#ifndef LLARP_LINK_ENCODER_HPP
#define LLARP_LINK_ENCODER_HPP

#include <llarp/bencode.h>
#include <llarp/buffer.h>
#include <llarp/router_contact.h>

namespace llarp
{
  /// encode Link Introduce Message onto a buffer
  /// if router is nullptr then the LIM's r member is omitted.
  bool
  EncodeLIM(llarp_buffer_t* buff, llarp_rc* router);
}  // namespace llarp

#endif
