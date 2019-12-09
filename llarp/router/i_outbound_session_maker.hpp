#ifndef LLARP_ROUTER_I_OUTBOUND_SESSION_MAKER_HPP
#define LLARP_ROUTER_I_OUTBOUND_SESSION_MAKER_HPP

#include <util/status.hpp>
#include <util/types.hpp>

#include <functional>

namespace llarp
{
  struct ILinkSession;
  struct RouterID;
  struct RouterContact;

  enum class SessionResult
  {
    Establish,
    Timeout,
    RouterNotFound,
    InvalidRouter,
    NoLink
  };

  using RouterCallback =
      std::function< void(const RouterID &, const SessionResult) >;

  struct IOutboundSessionMaker
  {
    virtual ~IOutboundSessionMaker() = default;

    virtual bool
    OnSessionEstablished(ILinkSession *session) = 0;

    virtual void
    OnConnectTimeout(ILinkSession *session) = 0;

    virtual void
    CreateSessionTo(const RouterID &router, RouterCallback on_result) = 0;

    virtual void
    CreateSessionTo(const RouterContact &rc, RouterCallback on_result) = 0;

    virtual bool
    HavePendingSessionTo(const RouterID &router) const = 0;

    virtual void
    ConnectToRandomRouters(int numDesired) = 0;

    virtual util::StatusObject
    ExtractStatus() const = 0;

    virtual bool
    ShouldConnectTo(const RouterID &router) const = 0;
  };

}  // namespace llarp

#endif  // LLARP_ROUTER_I_OUTBOUND_SESSION_MAKER_HPP
