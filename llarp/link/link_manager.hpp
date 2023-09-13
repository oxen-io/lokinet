#pragma once

#include <llarp/util/compare_ptr.hpp>
#include "server.hpp"
#include "endpoint.hpp"

#include <external/oxen-libquic/include/quic.hpp>

#include <unordered_map>
#include <set>
#include <atomic>

namespace llarp
{
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

  struct LinkManager
  {
   public:
    explicit LinkManager(AbstractRouter* r) : router{r}
    {}

    bool
    SendTo(
        const RouterID& remote,
        const llarp_buffer_t& buf,
        AbstractLinkSession::CompletionHandler completed,
        uint16_t priority);

    bool
    HaveConnection(const RouterID& remote, bool client_only = false) const;

    bool
    HaveClientConnection(const RouterID& remote) const;

    void
    DeregisterPeer(RouterID remote);

    void
    AddLink(const oxen::quic::opt::local_addr& bind, bool inbound = false);

    void
    Stop();

    void
    PersistSessionUntil(const RouterID& remote, llarp_time_t until);

    size_t
    NumberOfConnectedRouters(bool clients_only = false) const;

    size_t
    NumberOfConnectedClients() const;

    bool
    GetRandomConnectedRouter(RouterContact& router) const;

    void
    CheckPersistingSessions(llarp_time_t now);

    void
    updatePeerDb(std::shared_ptr<PeerDb> peerDb);

    util::StatusObject
    ExtractStatus() const;

    void
    Init(RCLookupHandler* rcLookup);

    void
    Connect(RouterID router);

    void
    Connect(RouterContact rc);

    // Attempts to connect to a number of random routers.
    //
    // This will try to connect to *up to* numDesired routers, but will not
    // check if we already have a connection to any of the random set, as making
    // that thread safe would be slow...I think.
    void
    ConnectToRandomRouters(int numDesired);

    // TODO: tune these (maybe even remove max?) now that we're switching to quic
    /// always maintain this many connections to other routers
    size_t minConnectedRouters = 4;
    /// hard upperbound limit on the number of router to router connections
    size_t maxConnectedRouters = 6;

   private:
    link::Endpoint*
    GetCompatibleLink(const RouterContact& rc);

    std::atomic<bool> stopping;
    mutable util::Mutex _mutex;  // protects m_PersistingSessions

    // sessions to persist -> timestamp to end persist at
    std::unordered_map<RouterID, llarp_time_t> m_PersistingSessions GUARDED_BY(_mutex);

    std::unordered_map<RouterID, SessionStats> m_lastRouterStats;

    util::DecayingHashSet<RouterID> m_Clients{path::default_lifetime};

    RCLookupHandler* _rcLookup;
    std::shared_ptr<NodeDB> _nodedb;

    AbstractRouter* router;

    // FIXME: Lokinet currently expects to be able to kill all network functionality before
    // finishing other shutdown things, including destroying this class, and that is all in
    // Network's destructor, so we need to be able to destroy it before this class.
    std::unique_ptr<oxen::quic::Network> quic{std::make_unique<oxen::quic::Network>()};

    std::vector<link::Endpoint> endpoints;

    // TODO: initialize creds
    std::shared_ptr<oxen::quic::GNUTLSCreds> tls_creds;

    void
    HandleIncomingDataMessage(oxen::quic::dgram_interface& dgi, bstring dgram);
    void
    HandleIncomingControlMessage(oxen::quic::Stream& stream, bstring_view packet);
  };

}  // namespace llarp
