#ifndef LLARP_MESSAGES_RELAY_HPP
#define LLARP_MESSAGES_RELAY_HPP
#include <llarp/link_message.hpp>

namespace llarp
{
  struct RelayUpstreamMessage : public ILinkMessage
  {
    RelayUpstreamMessage(const RouterID& from);
    ~RelayUpstreamMessage();

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    HandleMessage(llarp_router* router) const;
  };

  struct RelayDownstreamMessage : public ILinkMessage
  {
    RelayDownstreamMessage(const RouterID& from);
    ~RelayDownstreamMessage();

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    HandleMessage(llarp_router* router) const;
  };
}

#endif
