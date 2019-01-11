#ifndef LLARP_MESSAGES_RELAY_HPP
#define LLARP_MESSAGES_RELAY_HPP

#include <crypto.hpp>
#include <encrypted.hpp>
#include <link_message.hpp>
#include <path/path_types.hpp>

#include <vector>

namespace llarp
{
  struct RelayUpstreamMessage : public ILinkMessage
  {
    PathID_t pathid;
    Encrypted< MAX_LINK_MSG_SIZE - 128 > X;
    TunnelNonce Y;

    RelayUpstreamMessage();
    RelayUpstreamMessage(ILinkSession* from);
    ~RelayUpstreamMessage();

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    HandleMessage(llarp::Router* router) const override;

    void
    Clear() override;
  };

  struct RelayDownstreamMessage : public ILinkMessage
  {
    PathID_t pathid;
    Encrypted< MAX_LINK_MSG_SIZE - 128 > X;
    TunnelNonce Y;
    RelayDownstreamMessage();
    RelayDownstreamMessage(ILinkSession* from);
    ~RelayDownstreamMessage();

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) override;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    HandleMessage(llarp::Router* router) const override;

    void
    Clear() override;
  };
}  // namespace llarp

#endif
