#ifndef LLARP_MESSAGE_PATH_CONFIRM_HPP
#define LLARP_MESSAGE_PATH_CONFIRM_HPP

#include <routing/message.hpp>

namespace llarp
{
  namespace routing
  {
    struct PathConfirmMessage final : public IMessage
    {
      uint64_t pathLifetime = 0;
      uint64_t pathCreated  = 0;

      PathConfirmMessage() = default;
      PathConfirmMessage(uint64_t lifetime);
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
        pathLifetime = 0;
        pathCreated  = 0;
        version      = 0;
      }
    };
  }  // namespace routing
}  // namespace llarp

#endif
