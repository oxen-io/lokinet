#ifndef LLARP_MESSAGE_PATH_CONFIRM_HPP
#define LLARP_MESSAGE_PATH_CONFIRM_HPP

#include <llarp/routing/message.hpp>

namespace llarp
{
  namespace routing
  {
    struct PathConfirmMessage : public IMessage
    {
      uint64_t pathLifetime;
      uint64_t pathCreated;
      PathConfirmMessage();
      PathConfirmMessage(uint64_t lifetime);
      ~PathConfirmMessage(){};

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      bool
      BDecode(llarp_buffer_t* buf);

      bool
      HandleMessage(IMessageHandler* h) const;
    };
  }  // namespace routing
}  // namespace llarp

#endif