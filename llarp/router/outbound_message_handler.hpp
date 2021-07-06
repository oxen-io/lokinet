#pragma once

#include "i_outbound_message_handler.hpp"

#include <llarp/ev/ev.hpp>
#include <llarp/util/thread/queue.hpp>
#include <llarp/util/decaying_hashset.hpp>
#include <llarp/path/path_types.hpp>
#include <llarp/router_id.hpp>

#include <list>
#include <unordered_map>
#include <utility>
#include <queue>

struct llarp_buffer_t;

namespace llarp
{
  struct ILinkManager;
  struct I_RCLookupHandler;
  enum class SessionResult;

  struct OutboundMessageHandler final : public IOutboundMessageHandler
  {
   public:
    ~OutboundMessageHandler() override = default;

    OutboundMessageHandler(size_t maxQueueSize = MAX_OUTBOUND_QUEUE_SIZE);

    /* Called to queue a message to be sent to a router.
     *
     * If there is no session with the destination router, the message is added to a
     * pending session queue for that router.  If there is no pending session to that
     * router, one is created.
     *
     * If there is a session to the destination router, the message is placed on the shared
     * outbound message queue to be processed on Tick().
     *
     * When this class' Tick() is called, that queue is emptied and the messages there
     * are placed in their paths' respective individual queues.
     *
     * Returns false if encoding the message into a buffer fails, true otherwise.
     * A return value of true merely means we successfully processed the queue request,
     * so for example an invalid destination still yields a true return.
     */
    bool
    QueueMessage(const RouterID& remote, const ILinkMessage& msg, SendStatusHandler callback)
        override EXCLUDES(_mutex);

    /* Called once per event loop tick.
     *
     * Processes messages on the shared message queue into their paths' respective
     * individual queues.
     *
     * Removes the individual queues for paths which have died / expired, as informed by
     * QueueRemoveEmptyPath.
     *
     * Sends all routing messages that have been queued, indicated by pathid 0 when queued.
     * Sends messages from path queues until all are empty or a set cap has been reached.
     */
    void
    Tick() override;

    /* Called from outside this class to inform it that a path has died / expired
     * and its queue should be discarded.
     */
    void
    RemovePath(const PathID_t& pathid) override;

    util::StatusObject
    ExtractStatus() const override;

    void
    Init(ILinkManager* linkManager, I_RCLookupHandler* lookupHandler, EventLoop_ptr loop);

   private:
    using Message = std::pair<std::vector<byte_t>, SendStatusHandler>;

    /* A message that has been queued for sending, but not yet
     * processed into an individual path's message queue.
     */
    struct MessageQueueEntry
    {
      uint16_t priority;
      Message message;
      PathID_t pathid;
      RouterID router;

      bool
      operator<(const MessageQueueEntry& other) const
      {
        return other.priority < priority;
      }
    };

    struct MessageQueueStats
    {
      uint64_t queued = 0;
      uint64_t dropped = 0;
      uint64_t sent = 0;
      uint32_t queueWatermark = 0;

      uint32_t perTickMax = 0;
      uint32_t numTicks = 0;
    };

    using MessageQueue = std::priority_queue<MessageQueueEntry>;

    /* If a session is not yet created with the destination router for a message,
     * a special queue is created for that router and an attempt is made to
     * establish a session.  When this establish attempt concludes, either
     * the messages are then sent to that router immediately, on success, or
     * the messages are dropped and their send status callbacks are invoked with
     * the appropriate send status.
     */
    void
    OnSessionResult(const RouterID& router, const SessionResult result);

    /* queues a message's send result callback onto the event loop */
    void
    DoCallback(SendStatusHandler callback, SendStatus status);

    void
    QueueSessionCreation(const RouterID& remote);

    bool
    EncodeBuffer(const ILinkMessage& msg, llarp_buffer_t& buf);

    /* sends the message along to the link layer, and hopefully out to the network
     *
     * returns the result of the call to LinkManager::SendTo()
     */
    bool
    Send(const RouterID& remote, const Message& msg);

    /* Sends the message along to the link layer if we have a session to the remote
     *
     * returns the result of the Send() call, or false if no session.
     */
    bool
    SendIfSession(const RouterID& remote, const Message& msg);

    /* queues a message to the shared outbound message queue.
     *
     * If the queue is full, the message is dropped and the message's status
     * callback is invoked with a congestion status.
     *
     * When this class' Tick() is called, that queue is emptied and the messages there
     * are placed in their paths' respective individual queues.
     */
    bool
    QueueOutboundMessage(
        const RouterID& remote, Message&& msg, const PathID_t& pathid, uint16_t priority = 0);

    /* Processes messages on the shared message queue into their paths' respective
     * individual queues.
     */
    void
    ProcessOutboundQueue();

    /*
     * Sends all routing messages that have been queued, indicated by pathid 0 when queued.
     *
     * Sends messages from path queues until all are empty or a set cap has been reached.
     * This will send one message from each queue in a round-robin fashion such that they
     * all have roughly equal access to bandwidth.  A notion of priority may be introduced
     * at a later time, but for now only routing messages get priority.
     */
    void
    SendRoundRobin();

    /* Invoked when an outbound session establish attempt has concluded.
     *
     * If the outbound session was successfully created, sends any messages queued
     * for that destination along to it.
     *
     * If the session was unsuccessful, invokes the send status callbacks of those
     * queued messages and drops them.
     */
    void
    FinalizeSessionRequest(const RouterID& router, SendStatus status) EXCLUDES(_mutex);

    llarp::thread::Queue<MessageQueueEntry> outboundQueue;
    llarp::util::DecayingHashSet<PathID_t> recentlyRemovedPaths;
    bool removedSomePaths;

    mutable util::Mutex _mutex;  // protects pendingSessionMessageQueues

    std::unordered_map<RouterID, MessageQueue> pendingSessionMessageQueues GUARDED_BY(_mutex);

    std::unordered_map<PathID_t, MessageQueue> outboundMessageQueues;

    std::queue<PathID_t> roundRobinOrder;

    ILinkManager* _linkManager;
    I_RCLookupHandler* _lookupHandler;
    EventLoop_ptr _loop;

    util::ContentionKiller m_Killer;

    // paths cannot have pathid "0", so it can be used as the "pathid"
    // for non-traffic (control) messages, so they can be prioritized.
    static const PathID_t zeroID;

    MessageQueueStats m_queueStats;
  };

}  // namespace llarp
