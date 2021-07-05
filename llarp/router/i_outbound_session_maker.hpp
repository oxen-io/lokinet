#pragma once

#include <llarp/util/status.hpp>
#include <llarp/util/types.hpp>

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
    NoLink,
    EstablishFail
  };

  inline std::ostream&
  operator<<(std::ostream& out, const SessionResult& st)
  {
    switch (st)
    {
      case SessionResult::Establish:
        return out << "success";
      case SessionResult::Timeout:
        return out << "timeout";
      case SessionResult::NoLink:
        return out << "no link";
      case SessionResult::InvalidRouter:
        return out << "invalid router";
      case SessionResult::RouterNotFound:
        return out << "not found";
      case SessionResult::EstablishFail:
        return out << "establish failed";
    }
    return out << "???";
  }

  using RouterCallback = std::function<void(const RouterID&, const SessionResult)>;

  struct IOutboundSessionMaker
  {
    virtual ~IOutboundSessionMaker() = default;

    virtual bool
    OnSessionEstablished(ILinkSession* session) = 0;

    virtual void
    OnConnectTimeout(ILinkSession* session) = 0;

    virtual void
    CreateSessionTo(const RouterID& router, RouterCallback on_result) = 0;

    virtual void
    CreateSessionTo(const RouterContact& rc, RouterCallback on_result) = 0;

    virtual bool
    HavePendingSessionTo(const RouterID& router) const = 0;

    virtual void
    ConnectToRandomRouters(int numDesired) = 0;

    virtual util::StatusObject
    ExtractStatus() const = 0;

    virtual bool
    ShouldConnectTo(const RouterID& router) const = 0;
  };

}  // namespace llarp
