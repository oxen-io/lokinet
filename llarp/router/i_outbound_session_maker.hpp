#pragma once

#include <llarp/util/status.hpp>
#include <llarp/util/types.hpp>
#include <llarp/util/formattable.hpp>

#include <fmt/format.h>

#include <functional>

namespace llarp
{
  struct AbstractLinkSession;
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

  constexpr std::string_view
  ToString(SessionResult sr)
  {
    return sr == llarp::SessionResult::Establish     ? "success"sv
        : sr == llarp::SessionResult::Timeout        ? "timeout"sv
        : sr == llarp::SessionResult::NoLink         ? "no link"sv
        : sr == llarp::SessionResult::InvalidRouter  ? "invalid router"sv
        : sr == llarp::SessionResult::RouterNotFound ? "not found"sv
        : sr == llarp::SessionResult::EstablishFail  ? "establish failed"sv
                                                     : "???"sv;
  }
  template <>
  constexpr inline bool IsToStringFormattable<SessionResult> = true;

  using RouterCallback = std::function<void(const RouterID&, const SessionResult)>;

  struct IOutboundSessionMaker
  {
    virtual ~IOutboundSessionMaker() = default;

    virtual bool
    OnSessionEstablished(AbstractLinkSession* session) = 0;

    virtual void
    OnConnectTimeout(AbstractLinkSession* session) = 0;

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
