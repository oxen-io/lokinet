#pragma once

#include <llarp/crypto/encrypted.hpp>
#include <llarp/crypto/types.hpp>
#include "link_message.hpp"
#include <llarp/path/path_types.hpp>

#include <vector>

namespace llarp
{
  struct RelayUpstreamMessage : public ILinkMessage
  {
    Encrypted<MAX_LINK_MSG_SIZE - 128> X;
    TunnelNonce Y;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    HandleMessage(AbstractRouter* router) const override;

    void
    Clear() override;
    const char*

    Name() const override
    {
      return "RelayUpstream";
    }
    uint16_t
    Priority() const override
    {
      return 0;
    }
  };

  struct RelayDownstreamMessage : public ILinkMessage
  {
    Encrypted<MAX_LINK_MSG_SIZE - 128> X;
    TunnelNonce Y;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    HandleMessage(AbstractRouter* router) const override;

    void
    Clear() override;

    const char*
    Name() const override
    {
      return "RelayDownstream";
    }

    uint16_t
    Priority() const override
    {
      return 0;
    }
  };
}  // namespace llarp
