#include <llarp/messages/relay.hpp>

namespace llarp
{
  RelayUpstreamMessage::RelayUpstreamMessage(const RouterID &from)
      : ILinkMessage(from)
  {
  }

  RelayUpstreamMessage::~RelayUpstreamMessage()
  {
  }

  bool
  RelayUpstreamMessage::BEncode(llarp_buffer_t *buf) const
  {
    // TODO: implement me
    return false;
  }

  bool
  RelayUpstreamMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf)
  {
    return false;
  }

  bool
  RelayUpstreamMessage::HandleMessage(llarp_router *router) const
  {
    return false;
  }

  RelayDownstreamMessage::RelayDownstreamMessage(const RouterID &from)
      : ILinkMessage(from)
  {
  }

  RelayDownstreamMessage::~RelayDownstreamMessage()
  {
  }
  bool
  RelayDownstreamMessage::BEncode(llarp_buffer_t *buf) const
  {
    // TODO: implement me
    return false;
  }

  bool
  RelayDownstreamMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf)
  {
    return false;
  }

  bool
  RelayDownstreamMessage::HandleMessage(llarp_router *router) const
  {
    return false;
  }
}
