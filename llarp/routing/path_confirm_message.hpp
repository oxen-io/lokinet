#pragma once

#include "message.hpp"

namespace llarp
{
  namespace routing
  {
    struct PathConfirmMessage final : public IMessage
    {
      llarp_time_t pathLifetime = 0s;
      llarp_time_t pathCreated = 0s;

      PathConfirmMessage() = default;
      PathConfirmMessage(llarp_time_t lifetime);
      ~PathConfirmMessage() override = default;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;

      void
      Clear() override
      {
        pathLifetime = 0s;
        pathCreated = 0s;
        version = 0;
      }
    };
  }  // namespace routing
}  // namespace llarp
