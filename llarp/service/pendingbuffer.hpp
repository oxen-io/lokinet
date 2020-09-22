#ifndef LLARP_SERVICE_PENDINGBUFFER_HPP
#define LLARP_SERVICE_PENDINGBUFFER_HPP

#include <service/protocol.hpp>
#include <util/buffer.hpp>

#include <algorithm>
#include <iterator>
#include <vector>

namespace llarp
{
  namespace service
  {
    struct PendingBuffer
    {
      std::vector<byte_t> payload;
      ProtocolType protocol;

      PendingBuffer(const llarp_buffer_t& buf, ProtocolType t) : payload(buf.sz), protocol(t)
      {
        std::copy(buf.base, buf.base + buf.sz, std::back_inserter(payload));
      }

      ManagedBuffer
      Buffer()
      {
        return ManagedBuffer{llarp_buffer_t(payload)};
      }
    };
  }  // namespace service

}  // namespace llarp

#endif
