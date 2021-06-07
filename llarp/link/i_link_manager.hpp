#pragma once

#include "server.hpp"
#include <llarp/util/types.hpp>
#include <llarp/peerstats/peer_db.hpp>

#include <functional>
#include <optional>

struct llarp_buffer_t;

namespace llarp
{
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

    // it is fine to have both an inbound and outbound session with
    // another relay, and is useful for network testing.  This test
    // is more specific for use with "should we connect outbound?"
    virtual bool
    HasOutboundSessionTo(const RouterID& remote) const = 0;

    /// return true if the session with this pubkey is a client
    /// return false if the session with this pubkey is a router
    /// return std::nullopt we have no session with this pubkey
    virtual std::optional<bool>
    SessionIsClient(RouterID remote) const = 0;

    virtual void
    PumpLinks() = 0;

    virtual void
    AddLink(LinkLayer_ptr link, bool inbound = false) = 0;

    virtual bool
    StartLinks() = 0;

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

    virtual void
    ForEachOutboundLink(std::function<void(LinkLayer_ptr)> visit) const = 0;

    /// close all connections to this peer
    /// remove all link layer commits
    virtual void
    DeregisterPeer(RouterID remote) = 0;

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

    virtual void
    updatePeerDb(std::shared_ptr<PeerDb> peerDb) = 0;

    virtual util::StatusObject
    ExtractStatus() const = 0;
  };

}  // namespace llarp
