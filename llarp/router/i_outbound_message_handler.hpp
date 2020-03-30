#ifndef LLARP_ROUTER_I_OUTBOUND_MESSAGE_HANDLER_HPP
#define LLARP_ROUTER_I_OUTBOUND_MESSAGE_HANDLER_HPP

#include <util/status.hpp>

#include <cstdint>
#include <functional>

namespace llarp
{
  /**
     The result of sending an outbound message to a peer
   */
  enum class SendStatus
  {
    /** the operation was successful */
    Success,
    /** the operation timed out */
    Timeout,
    /** there was no appropriate link to deliver the message */
    NoLink,
    /** the router to deliver to is deemed invalid by network policy */
    InvalidRouter,
    /** the router to deliver to is not known and could not be looked up */
    RouterNotFound,
    /** network congestion prevented delivery */
    Congestion
  };

  struct ILinkMessage;
  struct RouterID;
  struct PathID_t;

  /** hook function for propagating SendStatus result to caller */
  using SendStatusHandler = std::function< void(SendStatus) >;

  /** maximum queue size for paths in number of messages */
  static const size_t MAX_PATH_QUEUE_SIZE = 40;
  /** maximum number of queues to flush per tick */
  static const size_t MAX_OUTBOUND_QUEUE_SIZE = 200;
  /** maxium number of messages to send per queue */
  static const size_t MAX_OUTBOUND_MESSAGES_PER_TICK = 20;

  /**
     Outbound Message Delivery Interface
     @brief handles queuing and delivery of outbound messages
   */
  struct IOutboundMessageHandler
  {
    virtual ~IOutboundMessageHandler() = default;

    /**
       @brief serialize and queue a message for delivery

       @param remote router id of router to deliver to
       @param msg a pointer to a LinkMessage to serialize
       @param callback hook to handle delivery
       @returns true if we successfully serialized and queued the message, false
       on serialize fail.
     */
    virtual bool
    QueueMessage(const RouterID &remote, const ILinkMessage *msg,
                 SendStatusHandler callback) = 0;

    /**
       @brief process queues
     */
    virtual void
    Tick() = 0;

    /**
       @brief remove path by path id from being processed
     */
    virtual void
    QueueRemoveEmptyPath(const PathID_t &pathid) = 0;

    /**
       @brief introspect for RPC
     */
    virtual util::StatusObject
    ExtractStatus() const = 0;
  };

}  // namespace llarp

#endif  // LLARP_ROUTER_I_OUTBOUND_MESSAGE_HANDLER_HPP
