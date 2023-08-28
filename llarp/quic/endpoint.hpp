#pragma once

#include "address.hpp"
#include "connection.hpp"
#include "io_result.hpp"
#include "null_crypto.hpp"
#include "packet.hpp"
#include "stream.hpp"
#include <llarp/net/ip_packet.hpp>

#include <chrono>
#include <map>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include <uvw/async.h>
#include <uvw/timer.h>

namespace llarp
{
  class EndpointBase;
}  // namespace llarp

namespace llarp::quic
{
  using namespace std::literals;

  inline constexpr auto IDLE_TIMEOUT = 5min;

  inline constexpr std::byte CLIENT_TO_SERVER{1};
  inline constexpr std::byte SERVER_TO_CLIENT{2};

  /// QUIC Tunnel Endpoint; this is the class that implements either end of a quic tunnel for both
  /// servers and clients.
  class Endpoint
  {
   public:
    /// Called from tun code via TunnelManager to deliver an incoming packet to us.
    ///
    /// \param src - the source address; this may be a tun interface address, or may be a fake IPv6
    /// address based on the convo tag.  The port is not used.
    /// \param ecn - the packet ecn parameter
    void
    receive_packet(const SockAddr& src, uint8_t ecn, bstring_view data);

    /// Returns a shared pointer to the uvw loop.
    std::shared_ptr<uvw::Loop>
    get_loop();

   protected:
    /// the service endpoint we are owned by
    EndpointBase& service_endpoint;

    /// local "address" is the IPv6 unspecified address since we don't have (or care about) the
    /// actual local address for building quic packets.  The port of this address must be set to our
    /// local pseudo-port, for clients, and 0 for a server.
    Address local_addr{in6addr_any};

    std::shared_ptr<uvw::TimerHandle> expiry_timer;

    std::vector<std::byte> buf;
    // Max theoretical size of a UDP packet is 2^16-1 minus IP/UDP header overhead
    static constexpr size_t max_buf_size = 64 * 1024;
    // Max size of a UDP packet that we'll send
    static constexpr size_t max_pkt_size_v4 = NGTCP2_MAX_UDP_PAYLOAD_SIZE;
    static constexpr size_t max_pkt_size_v6 = NGTCP2_MAX_UDP_PAYLOAD_SIZE;

    using primary_conn_ptr = std::shared_ptr<Connection>;
    using alias_conn_ptr = std::weak_ptr<Connection>;

    // Connections.  When a client establishes a new connection it chooses its own source connection
    // ID and a destination connection ID and sends them to the server.
    //
    // This container stores the primary Connection instance as a shared_ptr, and any connection
    // aliases as weak_ptrs referencing the primary instance (so that we don't have to double a
    // double-hash lookup on incoming packets, since those frequently use aliases).
    //
    // The destination connection ID should be entirely random and can be up to 160 bits, but the
    // source connection ID does not have to be (i.e. it can encode some information, if desired).
    //
    // The server is going to include in the response:
    // - destination connection ID equal to the client's source connection ID
    // - a new random source connection ID.  (We don't use the client's destination ID but generate
    // our own).  Like the clients source ID, this can contain embedded info.
    //
    // The client stores this, and so we end up with client-scid == server-dcid, and client-dcid ==
    // server-scid, where each side chose its own source connection ID.
    //
    // Ultimately, we store here our own {source connection ID -> Connection} pairs (or
    // equivalently, on incoming packets, the key will be the packet's dest conn ID).
    std::unordered_map<ConnectionID, std::variant<primary_conn_ptr, alias_conn_ptr>> conns;

    using conns_iterator = decltype(conns)::iterator;

    // Connections that are draining (i.e. we are dropping, but need to keep around for a while
    // to catch and drop lagged packets).  The time point is the scheduled removal time.
    std::queue<std::pair<ConnectionID, std::chrono::steady_clock::time_point>> draining;

    NullCrypto null_crypto;

    // Random data that we hash together with a CID to make a stateless reset token
    std::array<std::byte, 32> static_secret;

    friend class Connection;

    explicit Endpoint(EndpointBase& service_endpoint_);

    virtual ~Endpoint();

    // Version & connection id info that we can potentially extract when decoding a packet
    struct version_info
    {
      uint32_t version;
      const uint8_t* dcid;
      size_t dcid_len;
      const uint8_t* scid;
      size_t scid_len;
    };

    // Called to handle an incoming packet
    void
    handle_packet(const Packet& p);

    // Internal method: handles initial common packet decoding, returns the connection ID or nullopt
    // if decoding failed.
    std::optional<ConnectionID>
    handle_packet_init(const Packet& p);
    // Internal method: handles a packet sent to the given connection
    void
    handle_conn_packet(Connection& c, const Packet& p);

    // Accept a new incoming connection, i.e. pre-handshake.  Returns a nullptr if the connection
    // can't be created (e.g. because of invalid initial data), or if incoming connections are not
    // accepted by this endpoint (i.e. because it is not a Server instance, or because there are no
    // registered listen handlers).  The base class default returns nullptr.
    virtual std::shared_ptr<Connection>
    accept_initial_connection(const Packet&)
    {
      return nullptr;
    }

    // Reads a packet and handles various error conditions.  Returns an io_result.  Note that it is
    // possible for the conn_it to be erased from `conns` if the error code is anything other than
    // success (0) or NGTCP2_ERR_RETRY.
    io_result
    read_packet(const Packet& p, Connection& conn);

    // Writes the lokinet packet header to the beginning of `buf_`; the header is prepend to quic
    // packets to identify which quic server the packet should be delivered to and consists of:
    // - type [1 byte]: 1 for client->server packets; 2 for server->client packets (other values
    // reserved)
    // - port [2 bytes, network order]: client pseudoport (i.e. either a source or destination port
    // depending on type)
    // - ecn value [1 byte]: provided by ngtcp2.  (Only the lower 2 bits are actually used).
    //
    // \param psuedo_port - the remote's pseudo-port (will be 0 if the remote is a server, > 0 for
    // a client remote)
    // \param ecn - the ecn value from ngtcp2
    //
    // Returns the number of bytes written to buf_.
    virtual size_t
    write_packet_header(nuint16_t pseudo_port, uint8_t ecn) = 0;

    // Sends a packet to `to` containing `data`. Returns a non-error io_result on success,
    // an io_result with .error_code set to the errno of the failure on failure.
    io_result
    send_packet(const Address& to, bstring_view data, uint8_t ecn);

    // Wrapper around the above that takes a regular std::string_view (i.e. of chars) and recasts
    // it to an string_view of std::bytes.
    io_result
    send_packet(const Address& to, std::string_view data, uint8_t ecn)
    {
      return send_packet(
          to, bstring_view{reinterpret_cast<const std::byte*>(data.data()), data.size()}, ecn);
    }

    // Another wrapper taking a vector
    io_result
    send_packet(const Address& to, const std::vector<std::byte>& data, uint8_t ecn)
    {
      return send_packet(to, bstring_view{data.data(), data.size()}, ecn);
    }

    void
    send_version_negotiation(const ngtcp2_version_cid& vi, const Address& source);

    // Looks up a connection. Returns a shared_ptr (either copied for a primary connection, or
    // locked from an alias's weak pointer) if the connection was found or nullptr if not; and a
    // bool indicating whether this connection ID was an alias (true) or not (false).  [Note: the
    // alias value can be true even if the shared_ptr is null in the case of an expired alias that
    // hasn't yet been cleaned up].
    std::pair<std::shared_ptr<Connection>, bool>
    get_conn(const ConnectionID& cid);

    // Called to start closing (or continue closing) a connection by sending a connection close
    // response to any incoming packets.
    //
    // Takes the iterator to the connection pair from `conns` and optional error parameters: if
    // `application` is false (the default) then we do a hard connection close because of transport
    // error, if true we do a graceful application close. `close_reason` can be provided for
    // propagating reason for close to remote, defaults to empty string. For application closes the
    // code is application-defined; for hard closes the code should be one of the NGTCP2_*_ERROR
    // values.
    void
    close_connection(
        Connection& conn,
        uint64_t code = NGTCP2_NO_ERROR,
        bool application = false,
        std::string_view close_reason = ""sv);

    /// Puts a connection into draining mode (i.e. after getting a connection close).  This will
    /// keep the connection registered for the recommended 3*Probe Timeout, during which we drop
    /// packets that use the connection id and after which we will forget about it.
    void
    start_draining(Connection& conn);

    void
    check_timeouts();

    /// Deletes a connection from `conns`; if the connecion is a primary connection shared pointer
    /// then it is removed and clean_alias_conns() is immediately called to remove any aliases to
    /// the connection.  If the given connection is an alias connection then it is removed but no
    /// cleanup is performed.  Returns true if something was removed, false if the connection was
    /// not found.
    bool
    delete_conn(const ConnectionID& cid);

    /// Removes any connection id aliases that no longer have associated Connections.
    void
    clean_alias_conns();

    /// Creates a new, unused connection ID alias for the given connection; adds the alias to
    /// `conns` and returns the ConnectionID.
    ConnectionID
    add_connection_id(Connection& conn, size_t cid_length = ConnectionID::max_size());

   public:
    // Makes a deterministic stateless reset token for the given connection ID. Writes it to dest
    // (which must have NGTCP2_STATELESS_RESET_TOKENLEN bytes available).
    void
    make_stateless_reset_token(const ConnectionID& cid, unsigned char* dest);

    // Default stream buffer size for streams opened through this endpoint.
    size_t default_stream_buffer_size = 64 * 1024;

    // Packet buffer we use when constructing custom packets to fire over lokinet
    std::array<std::byte, net::IPPacket::MaxSize> buf_;

    // Non-copyable, non-movable
    Endpoint(const Endpoint&) = delete;
    Endpoint(Endpoint&&) = delete;
  };

}  // namespace llarp::quic
