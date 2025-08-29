#pragma once

#include "connection.hpp"

#include <llarp/contact/relay_contact.hpp>
#include <llarp/contact/router_id.hpp>
#include <llarp/util/time.hpp>

#include <oxen/quic/btstream.hpp>
#include <oxen/quic/endpoint.hpp>
#include <oxen/quic/gnutls_crypto.hpp>

#include <array>
#include <chrono>
#include <memory>

namespace llarp
{
    class Router;
}  // namespace llarp

namespace llarp::link
{
    class Manager;

    // How long we wait before closing the less-preferred redundant connection when we have
    // bidirection connections between two routers.  Once both sides have both directions, they
    // mutually determine the "winner" and send all future traffic on the winner connection.  This
    // buffer is to allow any messages or data to be handled that might have been send down the
    // less-preferred connection before both directions were established.
    //
    // We also use this value as a ticker interval, so redundant connections can stay alive up to
    // twice this value.
    inline constexpr auto REDUNDANT_LINGER = 20s;

    // Stores relay-to-relay connections.  In order to not lose stream messages, we temporarily
    // allow simultaneous connections in both directions between a pair of relays, but then
    // after a timeout, both sides choose the same winner and drop the other one.  The timeout
    // ensures that if we race to establish that we don't prematurely close while stream data is
    // still in flight (i.e. before both sides have aligned to the winning connection stream).
    struct relay_conn
    {
        // Constructor: takes an argument that is true if the inbound connection should take
        // precedence over the outbound conn when we have both, which is generally performed by
        // comparing rounter IDs (so as to be consistent on both sides).
        explicit relay_conn(bool inbound_wins) : inbound_wins{inbound_wins} {}

        bool inbound_wins;
        std::shared_ptr<link::Connection> inbound;
        std::shared_ptr<link::Connection> outbound;

        // Pointer to the current preferred connection, or nullptr if there is no current
        // connection:
        link::Connection* conn = nullptr;

        // Sets the appropriate inbound/outbound pointer and, if this is the only or the winning
        // connection, also sets it to `conn`.  If the existing inbound/outbound pointer is
        // already set, it is quietly closed before being replaced.
        void set_conn(std::shared_ptr<link::Connection> c, bool is_inbound);

        // Closes either the inbound or outbound connection and drops it from this instance.  If
        // the other connection still exists then `conn` is updated to point at it, otherwise it
        // is set to nullptr.  Does nothing if the indicated connection is already closed.
        void close_quietly(bool direction_inbound);

        // Closes all connections, in both directions (if opened).
        void close_all_quietly();

        // Closes the "loser" connection, if this instance has connections in both directions.
        void close_redundant();
    };

    class Endpoint
    {
      public:
        explicit Endpoint(Manager& lm);

        Manager& manager;
        Router& router;

      private:
        // Stores established relay-to-relay connections; only used by service nodes.
        std::unordered_map<RouterID, relay_conn> relay_conns;

        // Stores keys of relay_conns of any relays with which we have bidirectional connections
        // that will need closing of the less-preferred connection (after a timeout).  The value is
        // when the latest connection was stored (used for allowing a safety margin before closing
        // the redundant one).
        std::unordered_map<RouterID, std::chrono::milliseconds> relay_bidir;

        // Stores not-yet-established outbound connections to relays.  When the connection
        // established, it is removed from here and inserted into `client_conns` (clients) or
        // `relay_conns` (relays).
        std::unordered_map<RouterID, std::shared_ptr<link::Connection>> pending_outbound;

        // Stores established client-to-relay connections.  (That is, outbound connections for a
        // client, and inbound client connections for a relay).
        std::unordered_map<RouterID, std::shared_ptr<link::Connection>> client_conns;

        std::unique_ptr<quic::Loop> loop;
        std::shared_ptr<quic::Endpoint> endpoint;
        std::shared_ptr<quic::Ticker> redundancy_ticker;
        std::shared_ptr<quic::GNUTLSCreds> tls_creds;

      public:

        void start_tickers();

        // Returns the connection to the given relay.  If there are established connections in both
        // directions (i.e. when running as a relay), this returns the mutually preferred one.
        // Returns nullptr if there is no established connection with the given relay at all.
        link::Connection* get_relay_conn(const RouterID& relay) const;

        // Drops any redundant connections, i.e. where connections between two relays are
        // established in both directions and sufficient time has passed so ensure that all messages
        // are flowing on the mutually preferred connection.
        void close_redundant(std::chrono::milliseconds now = llarp::time_now_ms());

        link::Connection* get_client_conn(const RouterID&) const;

        // Returns a set of all relays with established (or pending, if `include_pending`)
        // connections.
        std::unordered_set<RouterID> get_current_relays(bool include_pending = false) const;

        // Returns true if there is an established (or pending, if `include_pending`) connection to
        // the given RouterID.
        bool connected_to_relay(const RouterID& relay, bool include_pending = false) const;

        // Returns 5-element array of relay connection counts.  (All values are 0 if this is called
        // on a client instance):
        // - number of relays with established connections.  Note that this counts each relay only
        //   once, i.e. if there are two bidirectional connections between two relays, it is not
        //   counted here twice.  Note that this also means this can be smaller than the sum of the
        //   next two values.
        // - number of established outbound relay-to-relay connections.
        // - number of inbound relay-to-relay connections.
        // - number of pending outbound connections.
        // - number of incoming connections from clients.
        std::array<int, 5> relay_connection_counts() const;

        // Returns the number of client->relay connections.  The first value is the number of
        // established connections, the second is the number of pending connections.
        std::array<int, 2> client_connection_counts() const;

        // Returns the number of relays we are connected to (for relays: relay-to-relay connections,
        // for clients this is simply the number of connections as all connections are to relays).
        // Does not double count relays (i.e. if connections exist in both directions, for
        // relay-to-relay connections).
        //
        // If `include_pending` is true then pending connections to relays are always included in
        // the count (but, as above, without double-counting relays in case the pending connection
        // would become a parallel connection).
        int num_relay_conns(bool include_pending = false) const;

        //        bool establish_connection(
        //            quic::RemoteAddress remote,
        //            RouterID rid,
        //            quic::connection_established_callback on_open = nullptr,
        //            quic::connection_closed_callback on_close = nullptr);

        // If there is no existing or pending connection to the given relay, initiates a new
        // outbound connection to it, otherwise does nothing.  Returns true a with the remote is
        // already established, false if it was initiated by this call or was already pending.
        bool ensure_connection(const RemoteRC& rc);

        // Returns a reference to the control stream currently in use to send commands to the given
        // relay.  If no connection exists yet with that relay, a new one is constructed (and so the
        // returned control stream might be on an not-yet-established connection).
        //
        // You often do *not* want to use this directly, because commands invoked it on it have
        // their callbacks fired in the network endpoint event loop rather than the router event
        // loop; instead see control_command for a wrapper that transfers callback execution to the
        // router loop.
        quic::BTRequestStream& control_stream_for(const RemoteRC& rc);

        // Sends a command on the control stream with `rc`, initiating a new connection if needed to
        // reach `rc`.  This is almost equivalent to `control_stream_for(rc).command(...)` except
        // that the callback, when it fires, is wrapped and transferred to the router loop rather
        // than executing in the endpoint event loop.
        void send_command(
            const RemoteRC& rc,
            std::string endpoint,
            std::vector<std::byte> body,
            std::function<void(quic::message)> response_handler);

        // Same as above, but takes an relay router id and looks it up.  If lookup fails, returns
        // false *without* calling the response handler.  Otherwise the message is sent to the
        // remote and the response (when triggered) fires on the router loop.
        bool send_command(
            const RouterID& rid,
            std::string endpoint,
            std::vector<std::byte> body,
            std::function<void(quic::message)> response_handler);

        // Send a data message (i.e. datagram) to the given remote.  Returns true if we were able to
        // queue the datagram for sending, false otherwise (such as when there is no fully
        // established connection to the given remote yet).  Unlike `control_command`, this does not
        // initiate a new connection if there is not already one established.
        bool send_datagram(const RouterID& remote, std::vector<std::byte> data);

        // Calls `func` for every relay connection.  If any relays have dual inbound/outbound
        // connections, this is only called for the preferred direction.
        void for_each_relay_conn(std::function<void(const RouterID&, link::Connection&)> func) const;

        void close_connection(const RouterID& rid);

        // Closes all connections and stops the network event loop
        void shutdown();

      private:
        std::shared_ptr<quic::BTRequestStream> make_control(quic::Connection& conn, const RouterID& rid);

        void on_inbound_conn(std::shared_ptr<quic::Connection> conn);
        void on_outbound_conn(std::shared_ptr<quic::Connection> conn);

        void on_conn_established(quic::Connection& conn);

        void on_conn_closed(quic::Connection& conn, uint64_t ec);

        std::pair<bool, quic::BTRequestStream*> ctrl_stream_impl(const RemoteRC& rc);
    };

}  // namespace llarp::link
