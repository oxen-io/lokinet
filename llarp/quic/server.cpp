#include "server.hpp"
#include <llarp/util/logging/buffer.hpp>
#include <llarp/util/logging/logger.hpp>

#include <oxenmq/hex.h>
#include <oxenmq/variant.h>
#include <uvw/loop.h>

#include <stdexcept>
#include <tuple>
#include <variant>

namespace llarp::quic
{
  std::shared_ptr<Connection>
  Server::accept_initial_connection(const Packet& p)
  {
    LogDebug("Accepting new connection");

    // This is a new incoming connection
    ngtcp2_pkt_hd hd;
    auto rv = ngtcp2_accept(&hd, u8data(p.data), p.data.size());

    if (rv == -1)
    {  // Invalid packet
      LogWarn("Invalid packet received, length=", p.data.size());
      LogTrace("packet body: ", buffer_printer{p.data});
      return nullptr;
    }

    if (rv == 1)
    {  // Invalid/unexpected version, send a version negotiation
      LogDebug("Invalid/unsupported version; sending version negotiation");
      send_version_negotiation(
          version_info{hd.version, hd.dcid.data, hd.dcid.datalen, hd.scid.data, hd.scid.datalen},
          p.path.remote);
      return nullptr;
    }

    if (hd.type == NGTCP2_PKT_0RTT)
    {
      LogWarn("Received 0-RTT packet, which shouldn't happen in our implementation; dropping");
      return nullptr;
    }

    if (hd.type == NGTCP2_PKT_INITIAL && hd.token.len)
    {
      // This is a normal QUIC thing, but we don't do it:
      LogWarn("Unexpected token in initial packet");
    }

    // create and store Connection
    for (;;)
    {
      if (auto [it, ins] = conns.emplace(ConnectionID::random(), primary_conn_ptr{}); ins)
      {
        auto connptr = std::make_shared<Connection>(*this, it->first, hd, p.path);
        it->second = connptr;
        LogDebug("Created local Connection ", it->first, " for incoming connection");
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
