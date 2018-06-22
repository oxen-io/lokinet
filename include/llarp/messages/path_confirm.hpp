#ifndef LLARP_MESSAGE_PATH_CONFIRM_HPP
#define LLARP_MESSAGE_PATH_CONFIRM_HPP

#include <llarp/routing_message.hpp>

namespace llarp
{
  namespace routing
  {
    struct PathConfirmMessage : public IMessage
    {
      uint64_t pathLifetime;
      uint64_t pathCreated;

      PathConfirmMessage(uint64_t lifetime);
      ~PathConfirmMessage(){};

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      BDecode(llarp_buffer_t* buf);

      bool
      HandleMessage(llarp_router* r) const;
    };
  }  // namespace routing
}  // namespace llarp

#endif