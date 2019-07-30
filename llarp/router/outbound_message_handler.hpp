#ifndef LLARP_ROUTER_OUTBOUND_MESSAGE_HANDLER_HPP
#define LLARP_ROUTER_OUTBOUND_MESSAGE_HANDLER_HPP

#include <router/i_outbound_message_handler.hpp>

#include <util/threading.hpp>
#include <util/logic.hpp>
#include <router_id.hpp>

#include <list>
#include <unordered_map>
#include <utility>

struct llarp_buffer_t;

namespace llarp
{
  struct ILinkManager;
  class Logic;
  enum class SessionResult;

  struct OutboundMessageHandler final : public IOutboundMessageHandler
  {
   public:
    ~OutboundMessageHandler() override = default;

    bool
    QueueMessage(const RouterID &remote, const ILinkMessage *msg,
                 SendStatusHandler callback) override LOCKS_EXCLUDED(_mutex);

    util::StatusObject
    ExtractStatus() const override;

    void
    Init(ILinkManager *linkManager, std::shared_ptr< Logic > logic);

   private:
    using Message      = std::pair< std::vector< byte_t >, SendStatusHandler >;
    using MessageQueue = std::list< Message >;

    void
    OnSessionEstablished(const RouterID &router);

    void
    OnConnectTimeout(const RouterID &router);

    void
    OnRouterNotFound(const RouterID &router);

    void
    OnInvalidRouter(const RouterID &router);

    void
    OnNoLink(const RouterID &router);

    void
    OnSessionResult(const RouterID &router, const SessionResult result);

    void
    DoCallback(SendStatusHandler callback, SendStatus status);

    void
    QueueSessionCreation(const RouterID &remote);

    bool
    EncodeBuffer(const ILinkMessage *msg, llarp_buffer_t &buf);

    bool
    Send(const RouterID &remote, const Message &msg);

    bool
    SendIfSession(const RouterID &remote, const Message &msg);

    void
    FinalizeRequest(const RouterID &router, SendStatus status)
        LOCKS_EXCLUDED(_mutex);

    mutable util::Mutex _mutex;  // protects outboundMessageQueue

    std::unordered_map< RouterID, MessageQueue, RouterID::Hash >
        outboundMessageQueue GUARDED_BY(_mutex);

    ILinkManager *_linkManager;
    std::shared_ptr< Logic > _logic;
  };

}  // namespace llarp

#endif  // LLARP_ROUTER_OUTBOUND_MESSAGE_HANDLER_HPP
