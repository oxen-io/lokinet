#ifndef LLARP_MESSAGE_PATH_CONFIRM_HPP
#define LLARP_MESSAGE_PATH_CONFIRM_HPP

#include <routing/message.hpp>

namespace llarp
{
  namespace routing
  {
    struct PathConfirmMessage final : public IMessage
    {
      uint64_t pathLifetime;
      uint64_t pathCreated;
      PathConfirmMessage();
      PathConfirmMessage(uint64_t lifetime);
      ~PathConfirmMessage(){};

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val) override;

      bool
      HandleMessage(IMessageHandler* h, llarp::Router* r) const override;

      void
      Clear() override{};
    };
  }  // namespace routing
}  // namespace llarp

#endif
