#ifndef LLARP_MESSAGES_LINK_INTRO_HPP
#define LLARP_MESSAGES_LINK_INTRO_HPP
#include <llarp/link_message.hpp>
#include <llarp/router_contact.hpp>
namespace llarp
{
  struct ILinkSession;

  struct LinkIntroMessage : public ILinkMessage
  {
    static constexpr size_t MaxSize = MAX_RC_SIZE + 256;

    LinkIntroMessage() : ILinkMessage()
    {
    }

    LinkIntroMessage(ILinkSession* s) : ILinkMessage(s)
    {
    }

    ~LinkIntroMessage();

    RouterContact rc;
    KeyExchangeNonce N;
    Signature Z;
    uint64_t P;

    LinkIntroMessage&
    operator=(const LinkIntroMessage& msg);

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    HandleMessage(llarp_router* router) const;

    bool
    Sign(llarp_crypto* c, const SecretKey& signKeySecret);

    bool
    Verify(llarp_crypto* c) const;
  };
}  // namespace llarp

#endif
