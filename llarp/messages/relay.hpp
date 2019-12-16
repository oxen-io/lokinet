#ifndef LLARP_MESSAGES_RELAY_HPP
#define LLARP_MESSAGES_RELAY_HPP

#include <crypto/encrypted.hpp>
#include <crypto/types.hpp>
#include <messages/link_message.hpp>
#include <path/path_types.hpp>

#include <vector>

namespace llarp
{
  struct RelayUpstreamMessage : public ILinkMessage
  {
    Encrypted< MAX_LINK_MSG_SIZE - 128 > X;
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
  };

  struct RelayDownstreamMessage : public ILinkMessage
  {
    Encrypted< MAX_LINK_MSG_SIZE - 128 > X;
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
  };
}  // namespace llarp

#endif
