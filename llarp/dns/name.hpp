#ifndef LLARP_DNS_NAME_HPP
#define LLARP_DNS_NAME_HPP

#include <net/net_int.hpp>
#include <util/buffer.hpp>

#include <string>

namespace llarp
{
  namespace dns
  {
    using Name_t = std::string;

    /// decode name from buffer
    bool
    DecodeName(llarp_buffer_t* buf, Name_t& name, bool trimTrailingDot = false);

    /// encode name to buffer
    bool
    EncodeName(llarp_buffer_t* buf, const Name_t& name);

    bool
    DecodePTR(const Name_t& name, huint32_t& ip);

  }  // namespace dns
}  // namespace llarp

#endif
