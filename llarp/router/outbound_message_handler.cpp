#include "outbound_message_handler.hpp"

#include <llarp/messages/link_message.hpp>
#include "router.hpp"
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
      const RouterID& remote, const AbstractLinkMessage& msg, SendStatusHandler callback)
  {
    // if the destination is invalid, callback with failure and return
    if (not _router->link_manager().have_client_connection_to(remote)
        and not _router->rc_lookup_handler().is_session_allowed(remote))
    {
      DoCallback(callback, SendStatus::InvalidRouter);
      return true;
    }
    MessageQueueEntry ent;
    ent.router = remote;
    ent.inform = std::move(callback);
    ent.pathid = msg.pathid;
    ent.priority = msg.priority();

    std::string _buf;
    _buf.reserve(MAX_LINK_MSG_SIZE);

    if (!EncodeBuffer(msg, _buf))
      return false;

    ent.message.resize(_buf.size());

    std::copy_n(_buf.data(), _buf.size(), ent.message.data());

    // if we have a session to the destination, queue the message and return
    if (_router->link_manager().have_connection_to(remote))
    {
      QueueOutboundMessage(std::move(ent));
      return true;
    }

    // if we don't have a session to the destination, queue the message onto
    // a special pending session queue for that destination, and then create
    // that pending session if there is not already a session establish attempt
    // in progress.
    bool shouldCreateSession = false;
    {
      util::Lock l{_mutex};

      // create queue for <remote> if it doesn't exist, and get iterator
      auto [queue_itr, is_new] = pendingSessionMessageQueues.emplace(remote, MessageQueue());
      queue_itr->second.push(std::move(ent));

      shouldCreateSession = is_new;
    }

    if (shouldCreateSession)
    {
      QueueSessionCreation(remote);
    }

    return true;
  }

  void
  OutboundMessageHandler::Pump()
  {
    m_Killer.TryAccess([this]() {
      recentlyRemovedPaths.Decay();
      ProcessOutboundQueue();
      // TODO: this probably shouldn't be pumping, as it defeats the purpose
      // of having a limit on sends per tick, but chaning it is potentially bad
      // and requires testing so it should be changed later.
      if (/*bool more = */ SendRoundRobin())
        _router->TriggerPump();
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
  OutboundMessageHandler::Init(Router* router)
  {
    _router = router;
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
        fmt::format("SessionResult {} has no corresponding SendStatus when transforming", result)};
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
      _router->loop()->call([f = std::move(callback), status] { f(status); });
  }

  // TODO: still necessary/desired?
  void
  OutboundMessageHandler::QueueSessionCreation(const RouterID& remote)
  {
    _router->link_manager().Connect(remote);
  }

  /** Note: This is where AbstractLinkMessage::bt_encode() is called. Contextually, this is
      different than how the other Abstract message types invoke ::bt_encode(), namely that
      there is no bt_dict_producer already being appended to. As a result, this use case
      likely requires a span backport and/or re-designed llarp_buffer. Until then, the
      ::bt_encode() override that returns an std::string upon destruction of its bt_dict_producer
      will be used
  */
  bool
  OutboundMessageHandler::EncodeBuffer(const AbstractLinkMessage& msg, std::string& buf)
  {
    if (buf = msg.bt_encode(); not buf.empty())
      return true;

    log::error(link_cat, "Error: OutboundMessageHandler failed to encode outbound message!");
    return false;
  }

  bool
  OutboundMessageHandler::Send(const MessageQueueEntry& ent)
  {
    const llarp_buffer_t buf{ent.message};
    m_queueStats.sent++;
    SendStatusHandler callback = ent.inform;
    return _router->link_manager().send_to(
        ent.router,
        buf,
        [this, callback](AbstractLinkSession::DeliveryStatus status) {
          if (status == AbstractLinkSession::DeliveryStatus::eDeliverySuccess)
            DoCallback(callback, SendStatus::Success);
          else
          {
            DoCallback(callback, SendStatus::Congestion);
          }
        },
        ent.priority);
  }

  bool
  OutboundMessageHandler::SendIfSession(const MessageQueueEntry& ent)
  {
    if (_router->link_manager().have_connection_to(ent.router))
    {
      return Send(ent);
    }
    return false;
  }

  bool
  OutboundMessageHandler::QueueOutboundMessage(MessageQueueEntry entry)
  {
    // copy callback in case we need to call it, so we can std::move(entry)
    auto callback = entry.inform;
    if (outboundQueue.tryPushBack(std::move(entry)) != llarp::thread::QueueReturn::Success)
    {
      m_queueStats.dropped++;
      DoCallback(callback, SendStatus::Congestion);
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
        continue;
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
        DoCallback(entry.inform, SendStatus::Congestion);
        m_queueStats.dropped++;
      }
    }
  }

  bool
  OutboundMessageHandler::SendRoundRobin()
  {
    m_queueStats.numTicks++;

    // send routing messages first priority
    auto& routing_mq = outboundMessageQueues[zeroID];
    while (not routing_mq.empty())
    {
      const MessageQueueEntry& entry = routing_mq.top();
      Send(entry);
      routing_mq.pop();
    }

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
    if (num_queues == 0)
    {
      return false;
    }

    // send messages for each pathid in roundRobinOrder, stopping when
    // either every path's queue is empty or a set maximum amount of
    // messages have been sent.
    size_t consecutive_empty = 0;
    for (size_t sent_count = 0; sent_count < MAX_OUTBOUND_MESSAGES_PER_TICK;)
    {
      PathID_t pathid = std::move(roundRobinOrder.front());
      roundRobinOrder.pop();

      auto& message_queue = outboundMessageQueues[pathid];
      if (message_queue.size() > 0)
      {
        const MessageQueueEntry& entry = message_queue.top();

        Send(entry);
        message_queue.pop();

        consecutive_empty = 0;
        consecutive_empty++;
      }
      else
      {
        consecutive_empty++;
      }

      roundRobinOrder.push(std::move(pathid));

      // if num_queues empty queues in a row, all queues empty.
      if (consecutive_empty == num_queues)
      {
        break;
      }
    }

    m_queueStats.perTickMax = std::max((uint32_t)consecutive_empty, m_queueStats.perTickMax);

    return consecutive_empty != num_queues;
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
        Send(entry);
      }
      else
      {
        DoCallback(entry.inform, status);
      }
      movedMessages.pop();
    }
  }

}  // namespace llarp
