#include "outbound_message_handler.hpp"

#include <llarp/messages/link_message.hpp>
#include "i_outbound_session_maker.hpp"
#include "i_rc_lookup_handler.hpp"
#include <llarp/link/i_link_manager.hpp>
#include <llarp/constants/link_layer.hpp>
#include <llarp/util/meta/memfn.hpp>
#include <llarp/util/status.hpp>

#include <algorithm>
#include <cstdlib>

namespace llarp
{
  const PathID_t OutboundMessageHandler::zeroID;

  using namespace std::chrono_literals;

  OutboundMessageHandler::OutboundMessageHandler(size_t maxQueueSize)
      : outboundQueue(maxQueueSize), recentlyRemovedPaths(5s), removedSomePaths(false)
  {}

  bool
  OutboundMessageHandler::QueueMessage(
      const RouterID& remote, const ILinkMessage& msg, SendStatusHandler callback)
  {
    // if the destination is invalid, callback with failure and return
    if (not _linkManager->SessionIsClient(remote) and not _lookupHandler->SessionIsAllowed(remote))
    {
      DoCallback(callback, SendStatus::InvalidRouter);
      return true;
    }
    const uint16_t priority = msg.Priority();
    std::array<byte_t, MAX_LINK_MSG_SIZE> linkmsg_buffer;
    llarp_buffer_t buf(linkmsg_buffer);

    if (!EncodeBuffer(msg, buf))
    {
      return false;
    }

    Message message;
    message.first.resize(buf.sz);
    message.second = callback;

    std::copy_n(buf.base, buf.sz, message.first.data());

    // if we have a session to the destination, queue the message and return
    if (_linkManager->HasSessionTo(remote))
    {
      QueueOutboundMessage(remote, std::move(message), msg.pathid, priority);
      return true;
    }

    // if we don't have a session to the destination, queue the message onto
    // a special pending session queue for that destination, and then create
    // that pending session if there is not already a session establish attempt
    // in progress.
    bool shouldCreateSession = false;
    {
      util::Lock l(_mutex);

      // create queue for <remote> if it doesn't exist, and get iterator
      auto [queue_itr, is_new] = pendingSessionMessageQueues.emplace(remote, MessageQueue());

      MessageQueueEntry entry;
      entry.priority = priority;
      entry.message = message;
      entry.router = remote;
      queue_itr->second.push(std::move(entry));

      shouldCreateSession = is_new;
    }

    if (shouldCreateSession)
    {
      QueueSessionCreation(remote);
    }

    return true;
  }

  void
  OutboundMessageHandler::Tick()
  {
    m_Killer.TryAccess([this]() {
      recentlyRemovedPaths.Decay();
      ProcessOutboundQueue();
      SendRoundRobin();
    });
  }

  void
  OutboundMessageHandler::RemovePath(const PathID_t& pathid)
  {
    m_Killer.TryAccess([this, pathid]() {
      /* add the path id to a list of recently removed paths to act as a filter
       * for messages that are queued but haven't been sorted into path queues yet.
       *
       * otherwise these messages would re-create the path queue we just removed, and
       * those path queues would be leaked / never removed.
       */
      recentlyRemovedPaths.Insert(pathid);
      auto itr = outboundMessageQueues.find(pathid);
      if (itr != outboundMessageQueues.end())
      {
        outboundMessageQueues.erase(itr);
      }
      removedSomePaths = true;
    });
  }

  util::StatusObject
  OutboundMessageHandler::ExtractStatus() const
  {
    util::StatusObject status{
        "queueStats",
        {{"queued", m_queueStats.queued},
         {"dropped", m_queueStats.dropped},
         {"sent", m_queueStats.sent},
         {"queueWatermark", m_queueStats.queueWatermark},
         {"perTickMax", m_queueStats.perTickMax},
         {"numTicks", m_queueStats.numTicks}}};

    return status;
  }

  void
  OutboundMessageHandler::Init(
      ILinkManager* linkManager, I_RCLookupHandler* lookupHandler, EventLoop_ptr loop)
  {
    _linkManager = linkManager;
    _lookupHandler = lookupHandler;
    _loop = std::move(loop);

    outboundMessageQueues.emplace(zeroID, MessageQueue());
  }

  static inline SendStatus
  ToSendStatus(const SessionResult result)
  {
    switch (result)
    {
      case SessionResult::Establish:
        return SendStatus::Success;
      case SessionResult::Timeout:
      case SessionResult::EstablishFail:
        return SendStatus::Timeout;
      case SessionResult::RouterNotFound:
        return SendStatus::RouterNotFound;
      case SessionResult::InvalidRouter:
        return SendStatus::InvalidRouter;
      case SessionResult::NoLink:
        return SendStatus::NoLink;
    }
    throw std::invalid_argument{
        stringify("SessionResult ", result, " has no corrispoding SendStatus when transforming")};
  }

  void
  OutboundMessageHandler::OnSessionResult(const RouterID& router, const SessionResult result)
  {
    FinalizeSessionRequest(router, ToSendStatus(result));
  }

  void
  OutboundMessageHandler::DoCallback(SendStatusHandler callback, SendStatus status)
  {
    if (callback)
      _loop->call([f = std::move(callback), status] { f(status); });
  }

  void
  OutboundMessageHandler::QueueSessionCreation(const RouterID& remote)
  {
    auto fn = util::memFn(&OutboundMessageHandler::OnSessionResult, this);
    _linkManager->GetSessionMaker()->CreateSessionTo(remote, fn);
  }

  bool
  OutboundMessageHandler::EncodeBuffer(const ILinkMessage& msg, llarp_buffer_t& buf)
  {
    if (!msg.BEncode(&buf))
    {
      LogWarn("failed to encode outbound message, buffer size left: ", buf.size_left());
      return false;
    }
    // set size of message
    buf.sz = buf.cur - buf.base;
    buf.cur = buf.base;

    return true;
  }

  bool
  OutboundMessageHandler::Send(const RouterID& remote, const Message& msg)
  {
    const llarp_buffer_t buf(msg.first);
    auto callback = msg.second;
    m_queueStats.sent++;
    return _linkManager->SendTo(remote, buf, [=](ILinkSession::DeliveryStatus status) {
      if (status == ILinkSession::DeliveryStatus::eDeliverySuccess)
        DoCallback(callback, SendStatus::Success);
      else
      {
        DoCallback(callback, SendStatus::Congestion);
      }
    });
  }

  bool
  OutboundMessageHandler::SendIfSession(const RouterID& remote, const Message& msg)
  {
    if (_linkManager->HasSessionTo(remote))
    {
      return Send(remote, msg);
    }
    return false;
  }

  bool
  OutboundMessageHandler::QueueOutboundMessage(
      const RouterID& remote, Message&& msg, const PathID_t& pathid, uint16_t priority)
  {
    MessageQueueEntry entry;
    entry.message = std::move(msg);

    // copy callback in case we need to call it, so we can std::move(entry)
    auto callback_copy = entry.message.second;
    entry.router = remote;
    entry.pathid = pathid;
    entry.priority = priority;
    if (outboundQueue.tryPushBack(std::move(entry)) != llarp::thread::QueueReturn::Success)
    {
      m_queueStats.dropped++;
      DoCallback(callback_copy, SendStatus::Congestion);
    }
    else
    {
      m_queueStats.queued++;

      uint32_t queueSize = outboundQueue.size();
      m_queueStats.queueWatermark = std::max(queueSize, m_queueStats.queueWatermark);
    }

    return true;
  }

  void
  OutboundMessageHandler::ProcessOutboundQueue()
  {
    while (not outboundQueue.empty())
    {
      MessageQueueEntry entry = outboundQueue.popFront();

      // messages may still be queued for processing when a pathid is removed,
      // so check here if the pathid was recently removed.
      if (recentlyRemovedPaths.Contains(entry.pathid))
      {
        return;
      }

      auto [queue_itr, is_new] = outboundMessageQueues.emplace(entry.pathid, MessageQueue());

      if (is_new && !entry.pathid.IsZero())
      {
        roundRobinOrder.push(entry.pathid);
      }

      MessageQueue& path_queue = queue_itr->second;

      if (path_queue.size() < MAX_PATH_QUEUE_SIZE || entry.pathid.IsZero())
      {
        path_queue.push(std::move(entry));
      }
      else
      {
        DoCallback(entry.message.second, SendStatus::Congestion);
        m_queueStats.dropped++;
      }
    }
  }

  void
  OutboundMessageHandler::SendRoundRobin()
  {
    m_queueStats.numTicks++;

    // send routing messages first priority
    auto& routing_mq = outboundMessageQueues[zeroID];
    while (not routing_mq.empty())
    {
      const MessageQueueEntry& entry = routing_mq.top();
      Send(entry.router, entry.message);
      routing_mq.pop();
    }

    size_t empty_count = 0;
    size_t num_queues = roundRobinOrder.size();

    // if any paths have been removed since last tick, remove any stale
    // entries from the round-robin ordering
    if (removedSomePaths)
    {
      for (size_t i = 0; i < num_queues; i++)
      {
        PathID_t pathid = std::move(roundRobinOrder.front());
        roundRobinOrder.pop();

        if (outboundMessageQueues.find(pathid) != outboundMessageQueues.end())
        {
          roundRobinOrder.push(std::move(pathid));
        }
      }
    }
    removedSomePaths = false;

    num_queues = roundRobinOrder.size();
    size_t sent_count = 0;
    if (num_queues == 0)  // if no queues, return
    {
      return;
    }

    // send messages for each pathid in roundRobinOrder, stopping when
    // either every path's queue is empty or a set maximum amount of
    // messages have been sent.
    while (sent_count < MAX_OUTBOUND_MESSAGES_PER_TICK)
    {
      PathID_t pathid = std::move(roundRobinOrder.front());
      roundRobinOrder.pop();

      auto& message_queue = outboundMessageQueues[pathid];
      if (message_queue.size() > 0)
      {
        const MessageQueueEntry& entry = message_queue.top();

        Send(entry.router, entry.message);
        message_queue.pop();

        empty_count = 0;
        sent_count++;
      }
      else
      {
        empty_count++;
      }

      roundRobinOrder.push(std::move(pathid));

      // if num_queues empty queues in a row, all queues empty.
      if (empty_count == num_queues)
      {
        break;
      }
    }

    m_queueStats.perTickMax = std::max((uint32_t)sent_count, m_queueStats.perTickMax);
  }

  void
  OutboundMessageHandler::FinalizeSessionRequest(const RouterID& router, SendStatus status)
  {
    MessageQueue movedMessages;
    {
      util::Lock l(_mutex);
      auto itr = pendingSessionMessageQueues.find(router);

      if (itr == pendingSessionMessageQueues.end())
      {
        return;
      }

      movedMessages.swap(itr->second);

      pendingSessionMessageQueues.erase(itr);
    }

    while (!movedMessages.empty())
    {
      const MessageQueueEntry& entry = movedMessages.top();

      if (status == SendStatus::Success)
      {
        Send(entry.router, entry.message);
      }
      else
      {
        DoCallback(entry.message.second, status);
      }
      movedMessages.pop();
    }
  }

}  // namespace llarp
