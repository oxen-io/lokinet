#pragma once

#include "connection.hpp"

#include <llarp/router/rc_lookup_handler.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/peerstats/peer_db.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/util/compare_ptr.hpp>

#include <quic.hpp>

#include <unordered_map>
#include <set>
#include <atomic>

#include <llarp/util/logging.hpp>
#include <llarp/util/priority_queue.hpp>

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

      // TODO: see which of these is actually useful and delete the other
      std::shared_ptr<link::Connection>
      get_conn(const RouterContact&) const;
      std::shared_ptr<link::Connection>
      get_conn(const RouterID&) const;

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

      void
      for_each_connection(std::function<void(link::Connection&)> func);

      void
      close_connection(RouterID rid);

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

  struct PendingMessage
  {
    std::string body;
    RouterID rid;
    bool is_control{false};

    PendingMessage(std::string b, bool control = false) : body{std::move(b)}, is_control{control}
    {}
  };

  struct PendingDataMessage : PendingMessage
  {
    PendingDataMessage(std::string b) : PendingMessage(b)
    {}
  };

  struct PendingControlMessage : PendingMessage
  {
    std::string endpoint;
    bool is_request{false};  // true if request, false if command

    PendingControlMessage(std::string b, std::string e, bool request = true)
        : PendingMessage(b, true), endpoint{std::move(e)}, is_request{request}
    {}
  };

  using MessageQueue = std::deque<PendingMessage>;

  struct Router;

  struct LinkManager
  {
   public:
    explicit LinkManager(Router& r);

    // set is_request to true for RPC requests, false for RPC commands
    bool
    send_control_message(
        const RouterID& remote, std::string endpoint, std::string body, bool is_request = true);

    bool
    send_data_message(const RouterID& remote, std::string data);

   private:
    friend struct link::Endpoint;

    const std::unordered_map<
        std::string,
        std::function<std::optional<std::string>(std::optional<std::string>)>>
        rpc_map{
            /** TODO:
                key: RPC endpoint name
                value: function that takes command body as parameter

                returns: commands will return std::nullopt while requests will return a response
            */
        };

    std::atomic<bool> is_stopping;
    // DISCUSS: is this necessary? can we reduce the amount of locking and nuke this
    mutable util::Mutex m;  // protects persisting_conns

    // sessions to persist -> timestamp to end persist at
    std::unordered_map<RouterID, llarp_time_t> persisting_conns GUARDED_BY(_mutex);

    // holds any messages we attempt to send while connections are establishing
    std::unordered_map<RouterID, MessageQueue> pending_conn_msg_queue;

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
    recv_control_message(oxen::quic::message msg);

    void
    on_conn_open(oxen::quic::connection_interface& ci);

    void
    on_conn_closed(oxen::quic::connection_interface& ci, uint64_t ec);

    std::shared_ptr<oxen::quic::Endpoint>
    startup_endpoint();

   public:
    const link::Endpoint&
    endpoint()
    {
      return ep;
    }

    const oxen::quic::Address&
    local()
    {
      return addr;
    }

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
    close_connection(RouterID rid);

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

    void
    for_each_connection(std::function<void(link::Connection&)> func);

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
        auto conn_interface =
            endpoint->connect(remote, link_manager.tls_creds, std::forward<Opt>(opts)...);

        // emplace immediately for connection open callback to find scid
        connid_map.emplace(conn_interface->scid(), rc.pubkey);
        auto [itr, b] = conns.emplace(rc.pubkey);

        auto control_stream =
            conn_interface->template get_new_stream<oxen::quic::BTRequestStream>();
        itr->second = std::make_shared<link::Connection>(conn_interface, rc, control_stream);

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

  llarp/router/outbound_message_handler.hpp
    - pendingsessionmessagequeue
      - establish queue of messages to be sent on a connection we are creating
      - upon creation, send these messages in the connection established callback
    - if connection times out, flush queue
    - TOCHECK: is priority used at all??

*/
