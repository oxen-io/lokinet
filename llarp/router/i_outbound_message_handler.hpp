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
  struct PathID_t;

  using SendStatusHandler = std::function< void(SendStatus) >;

  static const size_t MAX_PATH_QUEUE_SIZE            = 40;
  static const size_t MAX_OUTBOUND_QUEUE_SIZE        = 200;
  static const size_t MAX_OUTBOUND_MESSAGES_PER_TICK = 20;

  struct IOutboundMessageHandler
  {
    virtual ~IOutboundMessageHandler() = default;

    virtual bool
    QueueMessage(const RouterID &remote, const ILinkMessage *msg,
                 SendStatusHandler callback) = 0;

    virtual void
    Tick() = 0;

    virtual void
    QueueRemoveEmptyPath(const PathID_t &pathid) = 0;

    virtual util::StatusObject
    ExtractStatus() const = 0;
  };

}  // namespace llarp

#endif  // LLARP_ROUTER_I_OUTBOUND_MESSAGE_HANDLER_HPP
