#ifndef LLARP_ROUTER_I_OUTBOUND_MESSAGE_HANDLER_HPP
#define LLARP_ROUTER_I_OUTBOUND_MESSAGE_HANDLER_HPP

#include <util/status.hpp>

#include <cstdint>
#include <functional>

namespace llarp
{
  enum class SendStatus
  {
    Success,
    Timeout,
    NoLink,
    InvalidRouter,
    RouterNotFound,
    Congestion
  };

  struct ILinkMessage;
  struct RouterID;

  using SendStatusHandler = std::function< void(SendStatus) >;

  struct IOutboundMessageHandler
  {
    virtual ~IOutboundMessageHandler() = default;

    virtual bool
    QueueMessage(const RouterID &remote, const ILinkMessage *msg,
                 SendStatusHandler callback) = 0;

    virtual util::StatusObject
    ExtractStatus() const = 0;
  };

}  // namespace llarp

#endif  // LLARP_ROUTER_I_OUTBOUND_MESSAGE_HANDLER_HPP
