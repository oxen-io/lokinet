#ifndef LLARP_MESSAGES_RELAY_HPP
#define LLARP_MESSAGES_RELAY_HPP
#include <llarp/link_message.hpp>

#include <crypto.hpp>
#include <llarp/encrypted.hpp>
#include <llarp/path_types.hpp>
#include <vector>

namespace llarp
{
  struct RelayUpstreamMessage : public ILinkMessage
  {
    PathID_t pathid;
    Encrypted X;
    TunnelNonce Y;

    RelayUpstreamMessage();
    RelayUpstreamMessage(ILinkSession* from);
    ~RelayUpstreamMessage();

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    HandleMessage(llarp::Router* router) const;
  };

  struct RelayDownstreamMessage : public ILinkMessage
  {
    PathID_t pathid;
    Encrypted X;
    TunnelNonce Y;
    RelayDownstreamMessage();
    RelayDownstreamMessage(ILinkSession* from);
    ~RelayDownstreamMessage();

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    HandleMessage(llarp::Router* router) const;
  };
}  // namespace llarp

#endif
