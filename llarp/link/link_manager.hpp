#pragma once

#include "i_link_manager.hpp"

#include <llarp/util/compare_ptr.hpp>
#include "server.hpp"
#include "endpoint.hpp"

#include <external/oxen-libquic/include/quic.hpp>

#include <unordered_map>
#include <set>
#include <atomic>

namespace llarp
{
  struct IRouterContactManager;

  struct LinkManager final : public ILinkManager
  {
   public:
     LinkManager(AbstractRouter* r) : router(r) {}

    ~LinkManager() override = default;

    link::Endpoint*
    GetCompatibleLink(const RouterContact& rc) const override;

    IOutboundSessionMaker*
    GetSessionMaker() const override;

    bool
    SendTo(
        const RouterID& remote,
        const llarp_buffer_t& buf,
        ILinkSession::CompletionHandler completed,
        uint16_t priority) override;

    bool
    HaveConnection(const RouterID& remote) const override;

    bool
    HaveClientConnection(const RouterID& remote) const override;

    void
    DeregisterPeer(RouterID remote) override;

    void
    AddLink(oxen::quic::Address bind, bool inbound = false);

    void
    Stop() override;

    void
    PersistSessionUntil(const RouterID& remote, llarp_time_t until) override;

    //TODO: change for libquic Connections
    void
    ForEachPeer(std::function<void(const ILinkSession*, bool)> visit, bool randomize = false)
        const override;

    //TODO: change for libquic Connections
    void
    ForEachPeer(std::function<void(ILinkSession*)> visit) override;

    //TODO: change for libquic Endpoints
    void
    ForEachInboundLink(std::function<void(LinkLayer_ptr)> visit) const override;

    //TODO: change for libquic Endpoints
    void
    ForEachOutboundLink(std::function<void(LinkLayer_ptr)> visit) const override;

    size_t
    NumberOfConnectedRouters(bool clients_only = false) const override;

    size_t
    NumberOfConnectedClients() const override;

    bool
    GetRandomConnectedRouter(RouterContact& router) const override;

    void
    CheckPersistingSessions(llarp_time_t now) override;

    void
    updatePeerDb(std::shared_ptr<PeerDb> peerDb) override;

    util::StatusObject
    ExtractStatus() const override;

    void
    Init(I_RCLookupHandler* rcLookup);

    // Do an RC lookup for the given RouterID; the result will trigger
    // Connect(RouterContact) on success (or if we already have it), and will
    // trigger connection failure callback on lookup failure.
    void
    Connect(RouterID router);

    // Establish a connection to the remote `rc`.
    //
    // Connection established/failed callbacks should be invoked when either happens,
    // but this function should do nothing if already connected.
    void
    Connect(RouterContact rc);

    // Attempts to connect to a number of random routers.
    //
    // This will try to connect to *up to* numDesired routers, but will not
    // check if we already have a connection to any of the random set, as making
    // that thread safe would be slow...I think.
    void
    ConnectToRandomRouters(int numDesired);

    //TODO: tune these (maybe even remove max?) now that we're switching to quic
    /// always maintain this many connections to other routers
    size_t minConnectedRouters = 4;
    /// hard upperbound limit on the number of router to router connections
    size_t maxConnectedRouters = 6;

   private:

    std::atomic<bool> stopping;
    mutable util::Mutex _mutex;  // protects m_PersistingSessions

    // sessions to persist -> timestamp to end persist at
    std::unordered_map<RouterID, llarp_time_t> m_PersistingSessions GUARDED_BY(_mutex);

    std::unordered_map<RouterID, SessionStats> m_lastRouterStats;

    util::DecayingHashSet<RouterID> m_Clients{path::default_lifetime};

    I_RCLookupHandler* _rcLookup;
    std::shared_ptr<NodeDB> _nodedb;

    AbstractRouter* router;

    // FIXME: Lokinet currently expects to be able to kill all network functionality before
    // finishing other shutdown things, including destroying this class, and that is all in
    // Network's destructor, so we need to be able to destroy it before this class.
    std::unique_ptr<oxen::quic::Network> quic { std::make_unique<oxen::quic::Network>() };

    std::vector<link::Endpoint> endpoints;

    //TODO: initialize creds
    std::shared_ptr<oxen::quic::GNUTLSCreds> tls_creds;

    void HandleIncomingDataMessage(oxen::quic::dgram_interface& dgi, bstring dgram);
    void HandleIncomingControlMessage(oxen::quic::Stream& stream, bstring_view packet);

  };

}  // namespace llarp
