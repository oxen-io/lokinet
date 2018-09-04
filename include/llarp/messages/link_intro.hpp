#ifndef LLARP_MESSAGES_LINK_INTRO_HPP
#define LLARP_MESSAGES_LINK_INTRO_HPP
#include <llarp/link_message.hpp>
#include <llarp/router_contact.hpp>
namespace llarp
{
  struct ILinkSession;

  struct LinkIntroMessage : public ILinkMessage
  {
    LinkIntroMessage() : ILinkMessage()
    {
    }

    LinkIntroMessage(ILinkSession* s) : ILinkMessage(s)
    {
    }

    ~LinkIntroMessage();

    RouterContact rc;

    KeyExchangeNonce N;

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    HandleMessage(llarp_router* router) const;
  };
}  // namespace llarp

#endif
