#ifndef LLARP_MESSAGES_PATH_LATENCY_HPP
#define LLARP_MESSAGES_PATH_LATENCY_HPP

#include <llarp/routing/message.hpp>

namespace llarp
{
  namespace routing
  {
    struct PathLatencyMessage final : public IMessage
    {
      uint64_t T = 0;
      uint64_t L = 0;
      PathLatencyMessage();

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val) override;

      bool
      HandleMessage(IMessageHandler* h, llarp::Router* r) const override;
    };
  }  // namespace routing
}  // namespace llarp

#endif
