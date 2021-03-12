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
  Server::Server(
      service::Endpoint* parent,
      std::shared_ptr<uvw::Loop> loop,
      stream_open_callback_t stream_open)
      : Endpoint{parent, std::move(loop)}, stream_open_callback{std::move(stream_open)}
  {}

  void
  Server::handle_packet(const Packet& p)
  {
    LogDebug("Handling incoming server packet: ", buffer_printer{p.data});
    auto maybe_dcid = handle_packet_init(p);
    if (!maybe_dcid)
      return;
    auto& dcid = *maybe_dcid;

    // See if we have an existing connection already established for it
    LogDebug("Incoming connection id ", dcid);
    primary_conn_ptr connptr;
    if (auto conn_it = conns.find(dcid); conn_it != conns.end())
    {
      if (auto* wptr = std::get_if<alias_conn_ptr>(&conn_it->second))
      {
        connptr = wptr->lock();
        if (!connptr)
          LogDebug("CID is an expired alias");
        else
          LogDebug("CID is an alias for primary CID ", connptr->base_cid);
      }
      else
      {
        connptr = var::get<primary_conn_ptr>(conn_it->second);
        LogDebug("CID is primary");
      }
    }
    else
    {
      connptr = accept_connection(p);
    }

    if (!connptr)
    {
      LogWarn("invalid or expired connection, ignoring");
      return;
    }

    handle_conn_packet(*connptr, p);
  }

  std::shared_ptr<Connection>
  Server::accept_connection(const Packet& p)
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

    /*
    ngtcp2_cid ocid;
    ngtcp2_cid *pocid = nullptr;
    */
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

}  // namespace llarp::quic
