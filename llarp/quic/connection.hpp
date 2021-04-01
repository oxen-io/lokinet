#pragma once

#include "address.hpp"
#include "stream.hpp"
#include "io_result.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>
#include <unordered_set>
#include <map>

extern "C"
{
#include <ngtcp2/ngtcp2.h>
#include <sodium/randombytes.h>
}
#include <uvw/async.h>
#include <uvw/poll.h>
#include <uvw/timer.h>

namespace llarp::quic
{
  // We send and verify this in the initial connection and handshake; this is designed to allow
  // future changes (by either breaking or handling backwards compat).
  constexpr const std::array<uint8_t, 8> handshake_magic_bytes{
      'l', 'o', 'k', 'i', 'n', 'e', 't', 0x01};
  constexpr std::basic_string_view<uint8_t> handshake_magic{
      handshake_magic_bytes.data(), handshake_magic_bytes.size()};

  // Flow control window sizes for a buffer and individual streams:
  constexpr uint64_t CONNECTION_BUFFER = 1024 * 1024;
  constexpr uint64_t STREAM_BUFFER = 64 * 1024;
  // Max number of simultaneous streams we support over one connection
  constexpr uint64_t STREAM_LIMIT = 32;

  using bstring_view = std::basic_string_view<std::byte>;

  class Endpoint;
  class Server;
  class Client;

  struct alignas(size_t) ConnectionID : ngtcp2_cid
  {
    ConnectionID() = default;
    ConnectionID(const uint8_t* cid, size_t length);
    ConnectionID(const ConnectionID& c) = default;
    ConnectionID(ngtcp2_cid c) : ConnectionID(c.data, c.datalen)
    {}
    ConnectionID&
    operator=(const ConnectionID& c) = default;

    static constexpr size_t
    max_size()
    {
      return NGTCP2_MAX_CIDLEN;
    }
    static_assert(NGTCP2_MAX_CIDLEN <= std::numeric_limits<uint8_t>::max());

    bool
    operator==(const ConnectionID& other) const
    {
      return datalen == other.datalen && std::memcmp(data, other.data, datalen) == 0;
    }
    bool
    operator!=(const ConnectionID& other) const
    {
      return !(*this == other);
    }

    static ConnectionID
    random(size_t size = ConnectionID::max_size());
  };
  std::ostream&
  operator<<(std::ostream& o, const ConnectionID& c);

}  // namespace llarp::quic
namespace std
{
  template <>
  struct hash<llarp::quic::ConnectionID>
  {
    // We pick our own source_cid randomly, so it's a perfectly good hash already.
    size_t
    operator()(const llarp::quic::ConnectionID& c) const
    {
      static_assert(
          alignof(llarp::quic::ConnectionID) >= alignof(size_t)
          && offsetof(llarp::quic::ConnectionID, data) % sizeof(size_t) == 0);
      return *reinterpret_cast<const size_t*>(c.data);
    }
  };
}  // namespace std
namespace llarp::quic
{
  /// Returns the current (monotonic) time as a time_point
  inline auto
  get_time()
  {
    return std::chrono::steady_clock::now();
  }

  /// Converts a time_point as returned by get_time to a nanosecond timestamp (as ngtcp2 expects).
  inline uint64_t
  get_timestamp(const std::chrono::steady_clock::time_point& t = get_time())
  {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count();
  }

  // Stores an established connection between server/client.
  class Connection : public std::enable_shared_from_this<Connection>
  {
   private:
    struct connection_deleter
    {
      void
      operator()(ngtcp2_conn* c) const
      {
        ngtcp2_conn_del(c);
      }
    };

    // Packet data storage for a packet we are currently sending
    std::array<std::byte, NGTCP2_MAX_PKTLEN_IPV4> send_buffer{};
    size_t send_buffer_size = 0;
    ngtcp2_pkt_info send_pkt_info{};

    // Attempts to send the packet in `send_buffer`.  If sending blocks then we set up a write poll
    // on the socket to wait for it to become available, and return an io_result with `.blocked()`
    // set to true.  On other I/O errors we return the errno, and on successful sending we return a
    // "true" (i.e. no error code) io_result.
    io_result
    send();

    // Internal base method called invoked during construction to set up common client/server
    // settings.  dest_cid and path must already be set.
    std::tuple<ngtcp2_settings, ngtcp2_transport_params, ngtcp2_callbacks>
    init();

    // Event trigger used to queue packet processing for this connection
    std::shared_ptr<uvw::AsyncHandle> io_trigger;

    // Schedules a retransmit in the event loop (according to when ngtcp2 tells us we should)
    void
    schedule_retransmit();
    std::shared_ptr<uvw::TimerHandle> retransmit_timer;

    // The port the client wants to connect to on the server
    uint16_t tunnel_port = 0;

   public:
    // The endpoint that owns this connection
    Endpoint& endpoint;

    /// The primary connection id of this Connection.  This is the key of endpoint.conns that stores
    /// the actual shared_ptr (everything else in `conns` is a weak_ptr alias).
    const ConnectionID base_cid;

    /// The destination connection id we use to send to the other end; the remote end sets this as
    /// the source cid in the header.
    ConnectionID dest_cid;

    /// The underlying ngtcp2 connection object
    std::unique_ptr<ngtcp2_conn, connection_deleter> conn;

    /// The most recent Path we have to/from the remote
    Path path;

    /// True if we are draining (that is, we recently received a connection close from the other end
    /// and should discard everything that comes in on this connection).  Do not set this directly:
    /// instead call Endpoint::start_draining(conn).
    bool draining = false;

    /// True when we are closing; conn_buffer will contain the closing stanza.
    bool closing = false;

    /// Buffer where we store non-stream connection data, e.g. for initial transport params during
    /// connection and the closing stanza when disconnecting.
    std::basic_string<std::byte> conn_buffer;

    // Stores callbacks of active streams, indexed by our local source connection ID that we assign
    // when the connection is initiated.
    std::map<StreamID, std::shared_ptr<Stream>> streams;

    /// Constructs and initializes a new incoming connection
    ///
    /// \param server - the Server object that owns this connection
    /// \param base_cid - the local "primary" ConnectionID we use for this connection, typically
    /// random
    /// \param header - packet header that initiated the connection \param path - the network path
    /// to reach the remote
    Connection(
        Server& server, const ConnectionID& base_cid, ngtcp2_pkt_hd& header, const Path& path);

    /// Establishes a connection from the local Client to a remote Server
    /// \param client - the Endpoint object that owns this connection
    /// \param base_cid - the client's source (i.e. local) connection ID, typically random
    /// \param path - the network path to reach the remote
    /// \param tunnel_port - the port that this connection should tunnel to on the remote end
    Connection(Client& client, const ConnectionID& scid, const Path& path, uint16_t tunnel_port);

    // Non-movable, non-copyable:
    Connection(Connection&&) = delete;
    Connection(const Connection&) = delete;
    Connection&
    operator=(Connection&&) = delete;
    Connection&
    operator=(const Connection&) = delete;

    ~Connection();

    operator const ngtcp2_conn*() const
    {
      return conn.get();
    }
    operator ngtcp2_conn*()
    {
      return conn.get();
    }

    // If this connection's endpoint is a server, returns a pointer to it.  Otherwise returns
    // nullptr.
    Server*
    server();

    // If this connection's endpoint is a client, returns a pointer to it.  Otherwise returs
    // nullptr.
    Client*
    client();

    // Called to signal libuv that this connection has stuff to do
    void
    io_ready();
    // Called (via libuv) when it wants us to do our stuff. Call io_ready() to schedule this.
    void
    on_io_ready();

    int
    setup_server_crypto_initial();

    // Flush any streams with pending data. Note that, depending on available ngtcp2 state, we may
    // not fully flush all streams -- some streams can individually block while waiting for
    // confirmation.
    void
    flush_streams();

    // Called when a new stream is opened
    int
    stream_opened(StreamID id);

    // Called when data is received for a stream
    int
    stream_receive(StreamID id, bstring_view data, bool fin);

    // Called when a stream is closed
    void
    stream_closed(StreamID id, uint64_t app_error_code);

    // Called when stream data has been acknowledged and can be freed
    int
    stream_ack(StreamID id, size_t size);

    // Asks the endpoint for a new connection ID alias to use for this connection.  cidlen can be
    // used to specify the size of the cid (default is full size).
    ConnectionID
    make_alias_id(size_t cidlen = ConnectionID::max_size());

    // A callback to invoke when the connection handshake completes.  Will be cleared after being
    // called.
    std::function<void(Connection&)> on_handshake_complete;

    // Returns true iff this connection has completed a handshake with the remote end.
    bool
    get_handshake_completed();

    // Callback that is invoked whenever new streams become available: i.e. after handshaking, or
    // after existing streams are closed.  Note that this callback is invoked whenever the number of
    // available streams increases, even if it was initially non-zero before the increase.  To see
    // how many streams are currently available call `get_streams_available()` (it will always be at
    // least 1 when this callback is invoked).
    std::function<void(Connection&)> on_stream_available;

    // Returns the number of available streams that can currently be opened on the connection
    int
    get_streams_available();

    // Opens a stream over this connection; when the server receives this it attempts to establish a
    // TCP connection to the tunnel configured in the connection.  The data callback is invoked as
    // data is received on this stream.  The close callback is called if the stream is closed
    // (either by the remote, or locally after a stream->close() call).
    //
    // \param data_cb -- callback to invoke when data is received
    // \param close_cb -- callback to invoke when the connection is closed
    //
    // Throws a `std::runtime_error` if the stream creation fails (e.g. because the connection has
    // no free stream capacity).
    //
    // Returns a const reference to the stored Stream shared_ptr (so that the caller can decide
    // whether they want a copy or not).
    const std::shared_ptr<Stream>&
    open_stream(Stream::data_callback_t data_cb, Stream::close_callback_t close_cb);

    // Accesses the stream via its StreamID; throws std::out_of_range if the stream doesn't exist.
    const std::shared_ptr<Stream>&
    get_stream(StreamID s) const;

    // Internal methods that need to be publicly callable because we call them from C functions:
    int
    init_client();
    int
    recv_initial_crypto(std::basic_string_view<uint8_t> data);
    int
    recv_transport_params(std::basic_string_view<uint8_t> data);
    int
    send_magic(ngtcp2_crypto_level level);
    int
    send_transport_params(ngtcp2_crypto_level level);
    void
    complete_handshake();
  };

}  // namespace llarp::quic
