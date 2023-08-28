#pragma once

#include "endpoint.hpp"
#include "server.hpp"

#include <llarp/util/types.hpp>
#include <llarp/peerstats/peer_db.hpp>

#include <functional>
#include <optional>

struct llarp_buffer_t;

namespace llarp
{

  //TODO: do we still want this?
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


  struct RouterContact;
  struct ILinkSession;
  struct IOutboundSessionMaker;
  struct RouterID;

  using bstring = std::basic_string<std::byte>;
  using bstring_view = std::basic_string_view<std::byte>;

  struct ILinkManager
  {
    virtual ~ILinkManager() = default;

    virtual bool
    SendTo(
        const RouterID& remote,
        const llarp_buffer_t& buf,
        ILinkSession::CompletionHandler completed,
        uint16_t priority = 0) = 0;

    virtual bool
    HaveConnection(const RouterID& remote, bool client_only = false) const = 0;

    /// return true if we have a connection to the remote and it is not a relay,
    /// else return false
    virtual bool
    HaveClientConnection(const RouterID& remote) const = 0;

    virtual void
    Stop() = 0;

    virtual void
    PersistSessionUntil(const RouterID& remote, llarp_time_t until) = 0;

    /// close all connections to this peer
    /// remove all link layer commits
    virtual void
    DeregisterPeer(RouterID remote) = 0;

    virtual size_t
    NumberOfConnectedRouters(bool clients_only = false) const = 0;

    virtual size_t
    NumberOfConnectedClients() const = 0;

    virtual bool
    GetRandomConnectedRouter(RouterContact& router) const = 0;

    virtual void
    CheckPersistingSessions(llarp_time_t now) = 0;

    virtual void
    updatePeerDb(std::shared_ptr<PeerDb> peerDb) = 0;

    virtual util::StatusObject
    ExtractStatus() const = 0;

    // Do an RC lookup for the given RouterID; the result will trigger
    // Connect(RouterContact) on success (or if we already have it), and will
    // trigger connection failure callback on lookup failure.
    virtual void
    Connect(RouterID router) = 0;

    // Establish a connection to the remote `rc`.
    //
    // Connection established/failed callbacks should be invoked when either happens,
    // but this function should do nothing if already connected.
    virtual void
    Connect(RouterContact rc) = 0;
  };

}  // namespace llarp
