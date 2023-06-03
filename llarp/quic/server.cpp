#include "server.hpp"
#include <llarp/util/logging.hpp>
#include <llarp/util/logging/buffer.hpp>

#include <oxenc/variant.h>
#include <uvw/loop.h>

#include <stdexcept>
#include <tuple>
#include <variant>

namespace llarp::quic
{
  static auto logcat = log::Cat("quic");

  std::shared_ptr<Connection>
  Server::accept_initial_connection(const Packet& p)
  {
    log::debug(logcat, "Accepting new connection");

    // This is a new incoming connection
    ngtcp2_pkt_hd hd;
    auto rv = ngtcp2_accept(&hd, u8data(p.data), p.data.size());

    if (rv == NGTCP2_ERR_VERSION_NEGOTIATION)
    {  // Invalid/unexpected version, send a version negotiation
      log::debug(logcat, "Invalid/unsupported version; sending version negotiation");
      send_version_negotiation(
          ngtcp2_version_cid{
              hd.version, hd.dcid.data, hd.dcid.datalen, hd.scid.data, hd.scid.datalen},
          p.path.remote);
      return nullptr;
    }
    else if (rv < 0)
    {  // Invalid packet.  rv could be NGTCP2_ERR_RETRY but that will only
       // happen if the incoming packet is 0RTT which we don't use.
      log::warning(logcat, "Invalid packet received, length={}", p.data.size());
      log::trace(logcat, "packet body: {}", buffer_printer{p.data});
      return nullptr;
    }

    if (hd.type == NGTCP2_PKT_0RTT)
    {
      log::warning(
          logcat, "Received 0-RTT packet, which shouldn't happen in our implementation; dropping");
      return nullptr;
    }

    if (hd.type == NGTCP2_PKT_INITIAL && hd.token.len)
    {
      // This is a normal QUIC thing, but we don't do it:
      log::warning(logcat, "Unexpected token in initial packet");
    }

    // create and store Connection
    for (;;)
    {
      if (auto [it, ins] = conns.emplace(ConnectionID::random(), primary_conn_ptr{}); ins)
      {
        auto connptr = std::make_shared<Connection>(*this, it->first, hd, p.path);
        it->second = connptr;
        return connptr;
      }
    }
  }

  size_t
  Server::write_packet_header(nuint16_t pport, uint8_t ecn)
  {
    buf_[0] = SERVER_TO_CLIENT;
    std::memcpy(&buf_[1], &pport.n, 2);  // remote quic pseudo-port (network order u16)
    buf_[3] = std::byte{ecn};
    return 4;
  }

}  // namespace llarp::quic
