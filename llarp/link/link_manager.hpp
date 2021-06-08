#pragma once

#include "i_link_manager.hpp"

#include <llarp/util/compare_ptr.hpp>
#include "server.hpp"

#include <unordered_map>
#include <set>
#include <atomic>

namespace llarp
{
  struct IRouterContactManager;

  struct LinkManager final : public ILinkManager
  {
   public:
    ~LinkManager() override = default;

    LinkLayer_ptr
    GetCompatibleLink(const RouterContact& rc) const override;

    IOutboundSessionMaker*
    GetSessionMaker() const override;

    bool
    SendTo(
        const RouterID& remote,
        const llarp_buffer_t& buf,
        ILinkSession::CompletionHandler completed) override;

    bool
    HasSessionTo(const RouterID& remote) const override;

    bool
    HasOutboundSessionTo(const RouterID& remote) const override;

    std::optional<bool>
    SessionIsClient(RouterID remote) const override;

    void
    DeregisterPeer(RouterID remote) override;

    void
    PumpLinks() override;

    void
    AddLink(LinkLayer_ptr link, bool inbound = false) override;

    bool
    StartLinks() override;

    void
    Stop() override;

    void
    PersistSessionUntil(const RouterID& remote, llarp_time_t until) override;

    void
    ForEachPeer(std::function<void(const ILinkSession*, bool)> visit, bool randomize = false)
        const override;

    void
    ForEachPeer(std::function<void(ILinkSession*)> visit) override;

    void
    ForEachInboundLink(std::function<void(LinkLayer_ptr)> visit) const override;

    void
    ForEachOutboundLink(std::function<void(LinkLayer_ptr)> visit) const override;

    size_t
    NumberOfConnectedRouters() const override;

    size_t
    NumberOfConnectedClients() const override;

    size_t
    NumberOfPendingConnections() const override;

    bool
    GetRandomConnectedRouter(RouterContact& router) const override;

    void
    CheckPersistingSessions(llarp_time_t now) override;

    void
    updatePeerDb(std::shared_ptr<PeerDb> peerDb) override;

    util::StatusObject
    ExtractStatus() const override;

    void
    Init(IOutboundSessionMaker* sessionMaker);

   private:
    LinkLayer_ptr
    GetLinkWithSessionTo(const RouterID& remote) const;

    std::atomic<bool> stopping;
    mutable util::Mutex _mutex;  // protects m_PersistingSessions

    using LinkSet = std::set<LinkLayer_ptr, ComparePtr<LinkLayer_ptr>>;

    LinkSet outboundLinks;
    LinkSet inboundLinks;

    // sessions to persist -> timestamp to end persist at
    std::unordered_map<RouterID, llarp_time_t> m_PersistingSessions GUARDED_BY(_mutex);

    std::unordered_map<RouterID, SessionStats> m_lastRouterStats;

    util::DecayingHashSet<RouterID> m_Clients{path::default_lifetime};

    IOutboundSessionMaker* _sessionMaker;
  };

}  // namespace llarp
