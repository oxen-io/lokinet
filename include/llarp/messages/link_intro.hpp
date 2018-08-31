#ifndef LLARP_MESSAGES_LINK_INTRO_HPP
#define LLARP_MESSAGES_LINK_INTRO_HPP
#include <llarp/link_message.hpp>
namespace llarp
{
  struct LinkIntroMessage : public ILinkMessage
  {
    LinkIntroMessage(RouterContact* rc) : ILinkMessage(), RC(rc)
    {
    }

    ~LinkIntroMessage();

    RouterContact* RC;

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    HandleMessage(llarp_router* router) const;
  };
}  // namespace llarp

#endif
