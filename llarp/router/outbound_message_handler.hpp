#pragma once

#include <llarp/ev/ev.hpp>
#include <llarp/util/thread/queue.hpp>
#include <llarp/util/decaying_hashset.hpp>
#include <llarp/path/path_types.hpp>
#include <llarp/util/priority_queue.hpp>
#include <llarp/router_id.hpp>

#include <list>
#include <unordered_map>
#include <utility>

struct llarp_buffer_t;

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

  struct Router;
  enum class SessionResult;
  struct AbstractLinkMessage;
  struct RouterID;
  struct PathID_t;

  static const size_t MAX_PATH_QUEUE_SIZE = 100;
  static const size_t MAX_OUTBOUND_QUEUE_SIZE = 1000;
  static const size_t MAX_OUTBOUND_MESSAGES_PER_TICK = 500;

  using SendStatusHandler = std::function<void(SendStatus)>;

  struct OutboundMessageHandler final
  {
   public:
    ~OutboundMessageHandler() = default;

    OutboundMessageHandler(size_t maxQueueSize = MAX_OUTBOUND_QUEUE_SIZE);

    /* Called to queue a message to be sent to a router.
     *
     * If there is no session with the destination router, the message is added to a
     * pending session queue for that router.  If there is no pending session to that
     * router, one is created.
     *
     * If there is a session to the destination router, the message is placed on the shared
     * outbound message queue to be processed on Pump().
     *
     * When this class' Pump() is called, that queue is emptied and the messages there
     * are placed in their paths' respective individual queues.
     *
     * Returns false if encoding the message into a buffer fails, true otherwise.
     * A return value of true merely means we successfully processed the queue request,
     * so for example an invalid destination still yields a true return.
     */
    bool
    QueueMessage(const RouterID& remote, const AbstractLinkMessage& msg, SendStatusHandler callback)
        EXCLUDES(_mutex);

    /* Called when pumping output queues, typically scheduled via a call to Router::TriggerPump().
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
    Pump();

    /* Called from outside this class to inform it that a path has died / expired
     * and its queue should be discarded.
     */
    void
    RemovePath(const PathID_t& pathid);

    util::StatusObject
    ExtractStatus() const;

    void
    Init(Router* router);

   private:
    /* A message that has been queued for sending, but not yet
     * processed into an individual path's message queue.
     */
    struct MessageQueueEntry
    {
      uint16_t priority;
      std::vector<byte_t> message;
      SendStatusHandler inform;
      PathID_t pathid;
      RouterID router;

      bool
      operator>(const MessageQueueEntry& other) const
      {
        return priority > other.priority;
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

    using MessageQueue = util::ascending_priority_queue<MessageQueueEntry>;

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
    EncodeBuffer(const AbstractLinkMessage& msg, std::string& buf);

    /* sends the message along to the link layer, and hopefully out to the network
     *
     * returns the result of the call to LinkManager::SendTo()
     */
    bool
    Send(const MessageQueueEntry& ent);

    /* Sends the message along to the link layer if we have a session to the remote
     *
     * returns the result of the Send() call, or false if no session.
     */
    bool
    SendIfSession(const MessageQueueEntry& ent);

    /* queues a message to the shared outbound message queue.
     *
     * If the queue is full, the message is dropped and the message's status
     * callback is invoked with a congestion status.
     *
     * When this class' Pump() is called, that queue is emptied and the messages there
     * are placed in their paths' respective individual queues.
     */
    bool
    QueueOutboundMessage(MessageQueueEntry entry);

    /* Processes messages on the shared message queue into their paths' respective
     * individual queues.
     */
    void
    ProcessOutboundQueue();

    /*
     * Sends routing messages that have been queued, indicated by pathid 0 when queued.
     *
     * Sends messages from path queues until all are empty or a set cap has been reached.
     * This will send one message from each queue in a round-robin fashion such that they
     * all have roughly equal access to bandwidth.  A notion of priority may be introduced
     * at a later time, but for now only routing messages get priority.
     *
     * Returns true if there is more to send (i.e. we hit the limit before emptying all path
     * queues), false if all queues were drained.
     */
    bool
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

    Router* _router;

    util::ContentionKiller m_Killer;

    // paths cannot have pathid "0", so it can be used as the "pathid"
    // for non-traffic (control) messages, so they can be prioritized.
    static const PathID_t zeroID;

    MessageQueueStats m_queueStats;
  };

}  // namespace llarp
