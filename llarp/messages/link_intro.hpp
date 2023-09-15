#pragma once

#include <llarp/crypto/types.hpp>
#include "link_message.hpp"
#include <llarp/router_contact.hpp>

namespace llarp
{
  struct AbstractLinkSession;

  struct LinkIntroMessage final : public AbstractLinkMessage
  {
    static constexpr size_t MAX_MSG_SIZE = MAX_RC_SIZE + 256;

    LinkIntroMessage() : AbstractLinkMessage()
    {}

    RouterContact rc;
    KeyExchangeNonce nonce;
    Signature sig;
    uint64_t session_period;

    std::string
    bt_encode() const override;

    bool
    handle_message(Router* router) const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    sign(std::function<bool(Signature&, const llarp_buffer_t&)> signer);

    bool
    verify() const;

    void
    clear() override;

    const char*
    name() const override
    {
      return "LinkIntro";
    }

    // always first
    uint16_t
    priority() const override
    {
      return std::numeric_limits<uint16_t>::max();
    }
  };
}  // namespace llarp
