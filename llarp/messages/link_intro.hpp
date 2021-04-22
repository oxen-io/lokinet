#pragma once

#include <llarp/crypto/types.hpp>
#include "link_message.hpp"
#include <llarp/router_contact.hpp>

namespace llarp
{
  struct ILinkSession;

  struct LinkIntroMessage : public ILinkMessage
  {
    static constexpr size_t MaxSize = MAX_RC_SIZE + 256;

    LinkIntroMessage() : ILinkMessage()
    {}

    RouterContact rc;
    KeyExchangeNonce N;
    Signature Z;
    uint64_t P;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    HandleMessage(AbstractRouter* router) const override;

    bool
    Sign(std::function<bool(Signature&, const llarp_buffer_t&)> signer);

    bool
    Verify() const;

    void
    Clear() override;

    const char*
    Name() const override
    {
      return "LinkIntro";
    }

    // always first
    uint16_t
    Priority() const override
    {
      return std::numeric_limits<uint16_t>::max();
    }
  };
}  // namespace llarp
