#ifndef LLARP_MESSAGES_LINK_INTRO_HPP
#define LLARP_MESSAGES_LINK_INTRO_HPP

#include <link_message.hpp>
#include <router_contact.hpp>

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
    HandleMessage(llarp::Router* router) const;

    bool
    Sign(std::function< bool(Signature&, llarp_buffer_t) > signer);

    bool
    Verify(llarp::Crypto* c) const;
  };
}  // namespace llarp

#endif
