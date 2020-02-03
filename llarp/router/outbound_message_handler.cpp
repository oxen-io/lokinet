#include <router/outbound_message_handler.hpp>

#include <messages/link_message.hpp>
#include <router/i_outbound_session_maker.hpp>
#include <link/i_link_manager.hpp>
#include <constants/link_layer.hpp>
#include <util/meta/memfn.hpp>
#include <util/status.hpp>

#include <algorithm>
#include <cstdlib>

namespace llarp
{
  const PathID_t OutboundMessageHandler::zeroID;

  OutboundMessageHandler::OutboundMessageHandler(size_t maxQueueSize)
      : outboundQueue(maxQueueSize), removedPaths(20), removedSomePaths(false)
  {
  }

  bool
  OutboundMessageHandler::QueueMessage(const RouterID &remote,
                                       const ILinkMessage *msg,
                                       SendStatusHandler callback)
  {
    const uint16_t priority = msg->Priority();
    std::array< byte_t, MAX_LINK_MSG_SIZE > linkmsg_buffer;
    llarp_buffer_t buf(linkmsg_buffer);

    if(!EncodeBuffer(msg, buf))
    {
      return false;
    }

    Message message;
    message.first.resize(buf.sz);
    message.second = callback;

    std::copy_n(buf.base, buf.sz, message.first.data());

    if(_linkManager->HasSessionTo(remote))
    {
      QueueOutboundMessage(remote, std::move(message), msg->pathid, priority);
      return true;
    }

    bool shouldCreateSession = false;
    {
      util::Lock l(&_mutex);

      // create queue for <remote> if it doesn't exist, and get iterator
      auto itr_pair =
          pendingSessionMessageQueues.emplace(remote, MessageQueue());

      MessageQueueEntry entry;
      entry.priority = priority;
      entry.message  = message;
      entry.router   = remote;
      itr_pair.first->second.push(std::move(entry));

      shouldCreateSession = itr_pair.second;
    }

    if(shouldCreateSession)
    {
      QueueSessionCreation(remote);
    }

    return true;
  }

  void
  OutboundMessageHandler::Tick()
  {
    m_Killer.TryAccess([self = this]() {
      self->ProcessOutboundQueue();
      self->RemoveEmptyPathQueues();
      self->SendRoundRobin();
    });
  }

  void
  OutboundMessageHandler::QueueRemoveEmptyPath(const PathID_t &pathid)
  {
    m_Killer.TryAccess([self = this, pathid]() {
      if(self->removedPaths.full())
      {
        self->RemoveEmptyPathQueues();
      }
      self->removedPaths.pushBack(pathid);
    });
  }

  // TODO: this
  util::StatusObject
  OutboundMessageHandler::ExtractStatus() const
  {
    util::StatusObject status{"queueStats",
                              {{"queued", m_queueStats.queued},
                               {"dropped", m_queueStats.dropped},
                               {"sent", m_queueStats.sent},
                               {"queueWatermark", m_queueStats.queueWatermark},
                               {"perTickMax", m_queueStats.perTickMax},
                               {"numTicks", m_queueStats.numTicks}}};

    return status;
  }

  void
  OutboundMessageHandler::Init(ILinkManager *linkManager,
                               std::shared_ptr< Logic > logic)
  {
    _linkManager = linkManager;
    _logic       = logic;

    outboundMessageQueues.emplace(zeroID, MessageQueue());
  }

  void
  OutboundMessageHandler::OnSessionEstablished(const RouterID &router)
  {
    FinalizeSessionRequest(router, SendStatus::Success);
  }

  void
  OutboundMessageHandler::OnConnectTimeout(const RouterID &router)
  {
    FinalizeSessionRequest(router, SendStatus::Timeout);
  }

  void
  OutboundMessageHandler::OnRouterNotFound(const RouterID &router)
  {
    FinalizeSessionRequest(router, SendStatus::RouterNotFound);
  }

  void
  OutboundMessageHandler::OnInvalidRouter(const RouterID &router)
  {
    FinalizeSessionRequest(router, SendStatus::InvalidRouter);
  }

  void
  OutboundMessageHandler::OnNoLink(const RouterID &router)
  {
    FinalizeSessionRequest(router, SendStatus::NoLink);
  }

  void
  OutboundMessageHandler::OnSessionResult(const RouterID &router,
                                          const SessionResult result)
  {
    switch(result)
    {
      case SessionResult::Establish:
        OnSessionEstablished(router);
        break;
      case SessionResult::Timeout:
        OnConnectTimeout(router);
        break;
      case SessionResult::RouterNotFound:
        OnRouterNotFound(router);
        break;
      case SessionResult::InvalidRouter:
        OnInvalidRouter(router);
        break;
      case SessionResult::NoLink:
        OnNoLink(router);
        break;
      default:
        LogError("Impossible situation: enum class value out of bounds.");
        std::abort();
        break;
    }
  }

  void
  OutboundMessageHandler::DoCallback(SendStatusHandler callback,
                                     SendStatus status)
  {
    if(callback)
    {
      auto f = std::bind(callback, status);
      LogicCall(_logic, [self = this, f]() { self->m_Killer.TryAccess(f); });
    }
  }

  void
  OutboundMessageHandler::QueueSessionCreation(const RouterID &remote)
  {
    auto fn = util::memFn(&OutboundMessageHandler::OnSessionResult, this);
    _linkManager->GetSessionMaker()->CreateSessionTo(remote, fn);
  }

  bool
  OutboundMessageHandler::EncodeBuffer(const ILinkMessage *msg,
                                       llarp_buffer_t &buf)
  {
    if(!msg->BEncode(&buf))
    {
      LogWarn("failed to encode outbound message, buffer size left: ",
              buf.size_left());
      return false;
    }
    // set size of message
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;

    return true;
  }

  bool
  OutboundMessageHandler::Send(const RouterID &remote, const Message &msg)
  {
    const llarp_buffer_t buf(msg.first);
    auto callback = msg.second;
    m_queueStats.sent++;
    return _linkManager->SendTo(
        remote, buf, [=](ILinkSession::DeliveryStatus status) {
          if(status == ILinkSession::DeliveryStatus::eDeliverySuccess)
            DoCallback(callback, SendStatus::Success);
          else
          {
            LogWarn("Send outbound message handler dropped message");
            DoCallback(callback, SendStatus::Congestion);
          }
        });
  }

  bool
  OutboundMessageHandler::SendIfSession(const RouterID &remote,
                                        const Message &msg)
  {
    if(_linkManager->HasSessionTo(remote))
    {
      return Send(remote, msg);
    }
    return false;
  }

  bool
  OutboundMessageHandler::QueueOutboundMessage(const RouterID &remote,
                                               Message &&msg,
                                               const PathID_t &pathid,
                                               uint16_t priority)
  {
    MessageQueueEntry entry;
    entry.message      = std::move(msg);
    auto callback_copy = entry.message.second;
    entry.router       = remote;
    entry.pathid       = pathid;
    entry.priority     = priority;
    if(outboundQueue.tryPushBack(std::move(entry))
       != llarp::thread::QueueReturn::Success)
    {
      m_queueStats.dropped++;
      LogWarn(
          "QueueOutboundMessage outbound message handler dropped message on "
          "pathid=",
          pathid);
      DoCallback(callback_copy, SendStatus::Congestion);
    }
    else
    {
      m_queueStats.queued++;

      uint32_t queueSize = outboundQueue.size();
      m_queueStats.queueWatermark =
          std::max(queueSize, m_queueStats.queueWatermark);
    }

    return true;
  }

  void
  OutboundMessageHandler::ProcessOutboundQueue()
  {
    while(not outboundQueue.empty())
    {
      // TODO: can we add util::thread::Queue::front() for move semantics here?
      MessageQueueEntry entry = outboundQueue.popFront();

      auto itr_pair =
          outboundMessageQueues.emplace(entry.pathid, MessageQueue());

      if(itr_pair.second && !entry.pathid.IsZero())
      {
        roundRobinOrder.push(entry.pathid);
      }

      MessageQueue &path_queue = itr_pair.first->second;

      if(path_queue.size() < MAX_PATH_QUEUE_SIZE)
      {
        path_queue.push(std::move(entry));
      }
      else
      {
        LogWarn(
            "ProcessOutboundQueue outbound message handler dropped message on "
            "pathid=",
            entry.pathid);
        DoCallback(entry.message.second, SendStatus::Congestion);
        m_queueStats.dropped++;
      }
    }
  }

  void
  OutboundMessageHandler::RemoveEmptyPathQueues()
  {
    removedSomePaths = false;
    if(removedPaths.empty())
      return;

    while(not removedPaths.empty())
    {
      auto itr = outboundMessageQueues.find(removedPaths.popFront());
      if(itr != outboundMessageQueues.end())
      {
        outboundMessageQueues.erase(itr);
      }
    }
    removedSomePaths = true;
  }

  void
  OutboundMessageHandler::SendRoundRobin()
  {
    m_queueStats.numTicks++;

    // send non-routing messages first priority
    auto &non_routing_mq = outboundMessageQueues[zeroID];
    while(not non_routing_mq.empty())
    {
      const MessageQueueEntry &entry = non_routing_mq.top();
      Send(entry.router, entry.message);
      non_routing_mq.pop();
    }

    size_t empty_count = 0;
    size_t num_queues  = roundRobinOrder.size();

    if(removedSomePaths)
    {
      for(size_t i = 0; i < num_queues; i++)
      {
        PathID_t pathid = std::move(roundRobinOrder.front());
        roundRobinOrder.pop();

        if(outboundMessageQueues.find(pathid) != outboundMessageQueues.end())
        {
          roundRobinOrder.push(std::move(pathid));
        }
      }
    }

    num_queues        = roundRobinOrder.size();
    size_t sent_count = 0;
    if(num_queues == 0)  // if no queues, return
    {
      return;
    }

    while(sent_count
          < MAX_OUTBOUND_MESSAGES_PER_TICK)  // TODO: better stop condition
    {
      PathID_t pathid = std::move(roundRobinOrder.front());
      roundRobinOrder.pop();

      auto &message_queue = outboundMessageQueues[pathid];
      if(message_queue.size() > 0)
      {
        const MessageQueueEntry &entry = message_queue.top();

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
      if(empty_count == num_queues)
      {
        break;
      }
    }

    m_queueStats.perTickMax =
        std::max((uint32_t)sent_count, m_queueStats.perTickMax);
  }

  void
  OutboundMessageHandler::FinalizeSessionRequest(const RouterID &router,
                                                 SendStatus status)
  {
    MessageQueue movedMessages;
    {
      util::Lock l(&_mutex);
      auto itr = pendingSessionMessageQueues.find(router);

      if(itr == pendingSessionMessageQueues.end())
      {
        return;
      }

      movedMessages.swap(itr->second);

      pendingSessionMessageQueues.erase(itr);
    }

    while(!movedMessages.empty())
    {
      const MessageQueueEntry &entry = movedMessages.top();

      if(status == SendStatus::Success)
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
