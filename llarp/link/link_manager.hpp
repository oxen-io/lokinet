#pragma once

#include <llarp/util/compare_ptr.hpp>

#include "server.hpp"
#include "endpoint.hpp"

#include <external/oxen-libquic/include/quic.hpp>

#include <unordered_map>
#include <set>
#include <atomic>

#include <llarp/util/logging.hpp>

namespace
{
  static auto quic_cat = llarp::log::Cat("lokinet.quic");
}  // namespace

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

  struct Router;

  struct LinkManager
  {
   public:
    explicit LinkManager(Router& r);

    bool
    send_to(
        const RouterID& remote,
        const llarp_buffer_t& buf,
        AbstractLinkSession::CompletionHandler completed,
        uint16_t priority);

    bool
    have_connection_to(const RouterID& remote, bool client_only = false) const;

    bool
    have_client_connection_to(const RouterID& remote) const;

    void
    deregister_peer(RouterID remote);

    void
    connect_to(const oxen::quic::opt::local_addr& remote);

    void
    connect_to(RouterID router);

    void
    connect_to(RouterContact rc);

    void
    stop();

    void
    set_conn_persist(const RouterID& remote, llarp_time_t until);

    size_t
    get_num_connected(bool clients_only = false) const;

    size_t
    get_num_connected_clients() const;

    bool
    get_random_connected(RouterContact& router) const;

    void
    check_persisting_conns(llarp_time_t now);

    void
    update_peer_db(std::shared_ptr<PeerDb> peerDb);

    util::StatusObject
    ExtractStatus() const;

    void
    init(RCLookupHandler* rcLookup);

    // Attempts to connect to a number of random routers.
    //
    // This will try to connect to *up to* numDesired routers, but will not
    // check if we already have a connection to any of the random set, as making
    // that thread safe would be slow...I think.
    void
    connect_to_random(int numDesired);

    // TODO: tune these (maybe even remove max?) now that we're switching to quic
    /// always maintain this many connections to other routers
    size_t min_connected_routers = 4;
    /// hard upperbound limit on the number of router to router connections
    size_t max_connected_routers = 6;

   private:
    friend struct link::Endpoint;

    std::shared_ptr<link::Connection>
    get_compatible_link(const RouterContact& rc);

    std::atomic<bool> stopping;
    mutable util::Mutex _mutex;  // protects m_PersistingSessions

    // sessions to persist -> timestamp to end persist at
    std::unordered_map<RouterID, llarp_time_t> persisting_conns GUARDED_BY(_mutex);

    std::unordered_map<RouterID, SessionStats> last_router_stats;

    util::DecayingHashSet<RouterID> clients{path::default_lifetime};

    RCLookupHandler* _rcLookup;
    std::shared_ptr<NodeDB> _nodedb;

    Router& router;

    // FIXME: Lokinet currently expects to be able to kill all network functionality before
    // finishing other shutdown things, including destroying this class, and that is all in
    // Network's destructor, so we need to be able to destroy it before this class.
    std::unique_ptr<oxen::quic::Network> quic;
    std::shared_ptr<oxen::quic::GNUTLSCreds> tls_creds;
    link::Endpoint ep;

    void
    recv_data_message(oxen::quic::dgram_interface& dgi, bstring dgram);
    void
    recv_control_message(oxen::quic::Stream& stream, bstring_view packet);
  };

}  // namespace llarp
