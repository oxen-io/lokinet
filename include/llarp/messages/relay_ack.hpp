#ifndef LLARP_MESSAGES_RELAY_ACK_HPP
#define LLARP_MESSAGES_RELAY_ACK_HPP
#include <llarp/link_message.hpp>

namespace llarp
{
  struct LR_AckMessage : public ILinkMessage
  {
    LR_AckMessage(const RouterID& from);

    ~LR_AckMessage();

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    HandleMessage(llarp_router* router) const;
  };
}

#endif
