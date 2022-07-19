#include "client.hpp"
#include "tunnel.hpp"
#include <llarp/util/logging/buffer.hpp>
#include <llarp/util/logging.hpp>

#include <oxenc/variant.h>
#include <llarp/service/address.hpp>
#include <llarp/service/endpoint.hpp>
#include <llarp/ev/ev_libuv.hpp>

#include <variant>

namespace llarp::quic
{
  Client::Client(EndpointBase& ep, const SockAddr& remote, uint16_t pseudo_port) : Endpoint{ep}
  {
    default_stream_buffer_size =
        0;  // We steal uvw's provided buffers so don't need an outgoing data buffer

    // *Our* port; we stuff this in the llarp quic header so it knows how to target quic packets
    // back to *this* client.
    local_addr.port(ToNet(huint16_t{pseudo_port}));

    uint16_t tunnel_port = remote.getPort();
    if (tunnel_port == 0)
      throw std::logic_error{"Cannot tunnel to port 0"};

    // TODO: need timers for:
    //
    // - timeout (to disconnect if idle for too long)
    //
    // - probably don't need for lokinet tunnel: change local addr -- attempts to re-bind the local
    // socket
    //
    // - key_update_timer

    Path path{local_addr, remote};
    llarp::LogDebug("Connecting to ", remote);

    auto conn = std::make_shared<Connection>(*this, ConnectionID::random(), path, tunnel_port);
    conn->io_ready();
    conns.emplace(conn->base_cid, std::move(conn));
  }

  std::shared_ptr<Connection>
  Client::get_connection()
  {
    // A client only has one outgoing connection, so everything in conns should either be a
    // shared_ptr or weak_ptr to that same outgoing connection so we can just use the first one.
    auto it = conns.begin();
    if (it == conns.end())
      return nullptr;
    if (auto* wptr = std::get_if<alias_conn_ptr>(&it->second))
      return wptr->lock();
    return var::get<primary_conn_ptr>(it->second);
  }

  size_t
  Client::write_packet_header(nuint16_t, uint8_t ecn)
  {
    buf_[0] = CLIENT_TO_SERVER;
    auto pseudo_port = local_addr.port();
    std::memcpy(&buf_[1], &pseudo_port.n, 2);  // remote quic pseudo-port (network order u16)
    buf_[3] = std::byte{ecn};
    return 4;
  }
}  // namespace llarp::quic
