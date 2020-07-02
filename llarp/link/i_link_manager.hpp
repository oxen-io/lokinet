#ifndef LLARP_I_LINK_MANAGER_HPP
#define LLARP_I_LINK_MANAGER_HPP

#include <link/server.hpp>
#include <util/thread/logic.hpp>
#include <util/types.hpp>

#include <functional>

struct llarp_buffer_t;

namespace llarp
{
  using Logic_ptr = std::shared_ptr<Logic>;

  struct RouterContact;
  struct ILinkSession;
  struct IOutboundSessionMaker;
  struct RouterID;

  struct ILinkManager
  {
    virtual ~ILinkManager() = default;

    virtual LinkLayer_ptr
    GetCompatibleLink(const RouterContact& rc) const = 0;

    virtual IOutboundSessionMaker*
    GetSessionMaker() const = 0;

    virtual bool
    SendTo(
        const RouterID& remote,
        const llarp_buffer_t& buf,
        ILinkSession::CompletionHandler completed) = 0;

    virtual bool
    HasSessionTo(const RouterID& remote) const = 0;

    virtual void
    PumpLinks() = 0;

    virtual void
    AddLink(LinkLayer_ptr link, bool inbound = false) = 0;

    virtual bool
    StartLinks(Logic_ptr logic) = 0;

    virtual void
    Stop() = 0;

    virtual void
    PersistSessionUntil(const RouterID& remote, llarp_time_t until) = 0;

    virtual void
    ForEachPeer(
        std::function<void(const ILinkSession*, bool)> visit, bool randomize = false) const = 0;

    virtual void
    ForEachPeer(std::function<void(ILinkSession*)> visit) = 0;

    virtual void
    ForEachInboundLink(std::function<void(LinkLayer_ptr)> visit) const = 0;

    virtual size_t
    NumberOfConnectedRouters() const = 0;

    virtual size_t
    NumberOfConnectedClients() const = 0;

    virtual size_t
    NumberOfPendingConnections() const = 0;

    virtual bool
    GetRandomConnectedRouter(RouterContact& router) const = 0;

    virtual void
    CheckPersistingSessions(llarp_time_t now) = 0;

    virtual util::StatusObject
    ExtractStatus() const = 0;
  };

}  // namespace llarp

#endif  // LLARP_I_LINK_MANAGER_HPP
