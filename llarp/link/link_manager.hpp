#pragma once

#include <llarp/router/rc_lookup_handler.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/peerstats/peer_db.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/util/compare_ptr.hpp>

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
  struct LinkManager;

  namespace link
  {
    struct Connection;

    struct Endpoint
    {
      Endpoint(std::shared_ptr<oxen::quic::Endpoint> ep, LinkManager& lm)
          : endpoint{std::move(ep)}, link_manager{lm}
      {}

      std::shared_ptr<oxen::quic::Endpoint> endpoint;
      LinkManager& link_manager;

      // for outgoing packets, we route via RouterID; map RouterID->Connection
      // for incoming packets, we get a ConnectionID; map ConnectionID->RouterID
      std::unordered_map<RouterID, std::shared_ptr<link::Connection>> conns;
      std::unordered_map<oxen::quic::ConnectionID, RouterID> connid_map;

      std::shared_ptr<link::Connection>
      get_conn(const RouterContact&) const;

      bool
      have_conn(const RouterID& remote, bool client_only) const;

      bool
      deregister_peer(RouterID remote);

      size_t
      num_connected(bool clients_only) const;

      bool
      get_random_connection(RouterContact& router) const;
      // DISCUSS: added template to forward callbacks/etc to endpoint->connect(...).
      // This would be useful after combining link_manager with the redundant classes
      // listed below. As a result, link_manager would be holding all the relevant
      // callbacks, tls_creds, and other context required for endpoint management
      template <typename... Opt>
      bool
      establish_connection(const oxen::quic::Address& remote, RouterContact& rc, Opt&&... opts);

     private:
    };
  }  // namespace link

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
    send_to(const RouterID& remote, const llarp_buffer_t& buf, uint16_t priority);

    bool
    have_connection_to(const RouterID& remote, bool client_only = false) const;

    bool
    have_client_connection_to(const RouterID& remote) const;

    void
    deregister_peer(RouterID remote);

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
    extract_status() const;

    void
    init(RCLookupHandler* rcLookup);

    // Attempts to connect to a number of random routers.
    //
    // This will try to connect to *up to* num_conns routers, but will not
    // check if we already have a connection to any of the random set, as making
    // that thread safe would be slow...I think.
    void
    connect_to_random(int num_conns);

    // TODO: tune these (maybe even remove max?) now that we're switching to quic
    /// always maintain this many connections to other routers
    size_t min_connected_routers = 4;
    /// hard upperbound limit on the number of router to router connections
    size_t max_connected_routers = 6;

   private:
    friend struct link::Endpoint;

    std::atomic<bool> stopping;
    // DISCUSS: is this necessary? can we reduce the amount of locking and nuke this
    mutable util::Mutex m;  // protects persisting_conns

    // sessions to persist -> timestamp to end persist at
    std::unordered_map<RouterID, llarp_time_t> persisting_conns GUARDED_BY(_mutex);

    util::DecayingHashSet<RouterID> clients{path::default_lifetime};

    RCLookupHandler* rc_lookup;
    std::shared_ptr<NodeDB> node_db;

    oxen::quic::Address addr;

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

  namespace link
  {
    template <typename... Opt>
    bool
    Endpoint::establish_connection(
        const oxen::quic::Address& remote, RouterContact& rc, Opt&&... opts)
    {
      try
      {
        oxen::quic::dgram_data_callback dgram_cb =
            [this](oxen::quic::dgram_interface& dgi, bstring dgram) {
              link_manager.recv_data_message(dgi, dgram);
            };

        auto conn_interface =
            endpoint->connect(remote, link_manager.tls_creds, dgram_cb, std::forward<Opt>(opts)...);
        auto control_stream = conn_interface->get_new_stream();

        // TOFIX: get a real RouterID after refactoring RouterID
        RouterID rid;
        auto [itr, b] = conns.emplace(rid);
        itr->second = std::make_shared<link::Connection>(conn_interface, rc, control_stream);
        connid_map.emplace(conn_interface->scid(), rid);

        return true;
      }
      catch (...)
      {
        log::error(quic_cat, "Error: failed to establish connection to {}", remote);
        return false;
      }
    }
  }  // namespace link

}  // namespace llarp

/*
- Refactor RouterID to use gnutls info and maybe ConnectionID
- Combine routerID and connectionID to simplify mapping in llarp/link/endpoint.hpp
- Combine llarp/link/session.hpp into llarp/link/connection.hpp::Connection

- Combine llarp/link/server.hpp::ILinkLayer into llarp/link/endpoint.hpp::Endpoint
  - must maintain metadata storage, callbacks, etc
- If: one endpoint for ipv4 and ipv6
  - Then: can potentially combine:
    - llarp/link/endpoint.hpp
    - llarp/link/link_manager.hpp
    - llarp/link/outbound_message_handler.hpp
    - llarp/link/outbound_session_maker.hpp

  -> Yields mega-combo endpoint managing object?
    - Can avoid "kitchen sink" by greatly reducing complexity of implementation

*/
