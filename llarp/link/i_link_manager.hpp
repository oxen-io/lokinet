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

    virtual llarp::link::Endpoint*
    GetCompatibleLink(const RouterContact& rc) const = 0;

    virtual IOutboundSessionMaker*
    GetSessionMaker() const = 0;

    virtual bool
    SendTo(
        const RouterID& remote,
        const llarp_buffer_t& buf,
        ILinkSession::CompletionHandler completed,
        uint16_t priority = 0) = 0;

    virtual bool
    HaveConnection(const RouterID& remote) const = 0;

    /// return true if we have a connection to the remote and it is not a relay,
    /// else return false
    bool
    HaveClientConnection(const RouterID& remote) const = 0;

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
  };

}  // namespace llarp
