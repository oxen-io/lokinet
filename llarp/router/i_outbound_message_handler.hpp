#pragma once

#include <llarp/util/status.hpp>

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

  using SendStatusHandler = std::function<void(SendStatus)>;

  static const size_t MAX_PATH_QUEUE_SIZE = 100;
  static const size_t MAX_OUTBOUND_QUEUE_SIZE = 1000;
  static const size_t MAX_OUTBOUND_MESSAGES_PER_TICK = 500;

  struct IOutboundMessageHandler
  {
    virtual ~IOutboundMessageHandler() = default;

    virtual bool
    QueueMessage(const RouterID& remote, const ILinkMessage& msg, SendStatusHandler callback) = 0;

    virtual void
    Tick() = 0;

    virtual void
    RemovePath(const PathID_t& pathid) = 0;

    virtual util::StatusObject
    ExtractStatus() const = 0;
  };

}  // namespace llarp
