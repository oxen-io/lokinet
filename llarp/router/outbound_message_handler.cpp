#include <router/outbound_message_handler.hpp>

#include <messages/link_message.hpp>
#include <router/i_outbound_session_maker.hpp>
#include <link/i_link_manager.hpp>
#include <constants/link_layer.hpp>
#include <util/memfn.hpp>
#include <util/status.hpp>

#include <algorithm>
#include <cstdlib>

namespace llarp
{
  bool
  OutboundMessageHandler::QueueMessage(const RouterID &remote,
                                       const ILinkMessage *msg,
                                       SendStatusHandler callback)
  {
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

    if(SendIfSession(remote, message))
    {
      return true;
    }

    bool shouldCreateSession = false;
    {
      util::Lock l(&_mutex);

      // create queue for <remote> if it doesn't exist, and get iterator
      auto itr_pair = outboundMessageQueue.emplace(remote, MessageQueue());

      itr_pair.first->second.push_back(std::move(message));

      shouldCreateSession = itr_pair.second;
    }

    if(shouldCreateSession)
    {
      QueueSessionCreation(remote);
    }

    return true;
  }

  // TODO: this
  util::StatusObject
  OutboundMessageHandler::ExtractStatus() const
  {
    util::StatusObject status{};
    return status;
  }

  void
  OutboundMessageHandler::Init(ILinkManager *linkManager,
                               std::shared_ptr< Logic > logic)
  {
    _linkManager = linkManager;
    _logic       = logic;
  }

  void
  OutboundMessageHandler::OnSessionEstablished(const RouterID &router)
  {
    FinalizeRequest(router, SendStatus::Success);
  }

  void
  OutboundMessageHandler::OnConnectTimeout(const RouterID &router)
  {
    FinalizeRequest(router, SendStatus::Timeout);
  }

  void
  OutboundMessageHandler::OnRouterNotFound(const RouterID &router)
  {
    FinalizeRequest(router, SendStatus::RouterNotFound);
  }

  void
  OutboundMessageHandler::OnInvalidRouter(const RouterID &router)
  {
    FinalizeRequest(router, SendStatus::InvalidRouter);
  }

  void
  OutboundMessageHandler::OnNoLink(const RouterID &router)
  {
    FinalizeRequest(router, SendStatus::NoLink);
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
      auto func = std::bind(callback, status);
      _logic->queue_func(func);
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
    return _linkManager->SendTo(
        remote, buf, [=](ILinkSession::DeliveryStatus status) {
          if(status == ILinkSession::DeliveryStatus::eDeliverySuccess)
            DoCallback(callback, SendStatus::Success);
          else
            DoCallback(callback, SendStatus::Congestion);
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

  void
  OutboundMessageHandler::FinalizeRequest(const RouterID &router,
                                          SendStatus status)
  {
    MessageQueue movedMessages;
    {
      util::Lock l(&_mutex);
      auto itr = outboundMessageQueue.find(router);

      if(itr == outboundMessageQueue.end())
      {
        return;
      }

      movedMessages.splice(movedMessages.begin(), itr->second);

      outboundMessageQueue.erase(itr);
    }

    for(const auto &msg : movedMessages)
    {
      if(status == SendStatus::Success)
      {
        Send(router, msg);
      }
      else
      {
        DoCallback(msg.second, status);
      }
    }
  }

}  // namespace llarp
