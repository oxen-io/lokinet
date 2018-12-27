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

    ~LinkIntroMessage();

    RouterContact rc;
    KeyExchangeNonce N;
    Signature Z;
    uint64_t P;

    LinkIntroMessage&
    operator=(const LinkIntroMessage& msg);

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    HandleMessage(llarp::Router* router) const override;

    bool
    Sign(llarp::Crypto* c, const SecretKey& signKeySecret);

    bool
    Verify(llarp::Crypto* c) const;

    void
    Clear() override;
  };
}  // namespace llarp

#endif
