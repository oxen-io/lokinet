#pragma once

#include "message.hpp"

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
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      void
      Clear() override
      {
        T = 0;
        L = 0;
        version = 0;
      }

      bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;
    };
  }  // namespace routing
}  // namespace llarp
