#ifndef LLARP_MESSAGES_LINK_INTRO_HPP
#define LLARP_MESSAGES_LINK_INTRO_HPP

#include <crypto/types.hpp>
#include <messages/link_message.hpp>
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
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    HandleMessage(AbstractRouter* router) const override;

    bool
    Sign(std::function< bool(Signature&, const llarp_buffer_t&) > signer);

    bool
    Verify(llarp::Crypto* c) const;

    void
    Clear() override;

    const char*
    Name() const override
    {
      return "LinkIntro";
    }
  };
}  // namespace llarp

#endif
