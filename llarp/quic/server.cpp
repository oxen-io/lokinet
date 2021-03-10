#include "server.hpp"
#include "log.hpp"

#include <oxenmq/hex.h>
#include <oxenmq/variant.h>
#include <uvw/loop.h>

#include <stdexcept>
#include <tuple>
#include <variant>

namespace llarp::quic
{
  Server::Server(
      Address listen, std::shared_ptr<uvw::Loop> loop, stream_open_callback_t stream_open)
      : Endpoint{std::move(listen), std::move(loop)}, stream_open_callback{std::move(stream_open)}
  {}

  void
  Server::handle_packet(const Packet& p)
  {
    Debug("Handling incoming server packet: ", buffer_printer{p.data});
    auto maybe_dcid = handle_packet_init(p);
    if (!maybe_dcid)
      return;
    auto& dcid = *maybe_dcid;

    // See if we have an existing connection already established for it
    Debug("Incoming connection id ", dcid);
    primary_conn_ptr connptr;
    if (auto conn_it = conns.find(dcid); conn_it != conns.end())
    {
      if (auto* wptr = std::get_if<alias_conn_ptr>(&conn_it->second))
      {
        connptr = wptr->lock();
        if (!connptr)
          Debug("CID is an expired alias");
        else
          Debug("CID is an alias for primary CID ", connptr->base_cid);
      }
      else
      {
        connptr = var::get<primary_conn_ptr>(conn_it->second);
        Debug("CID is primary");
      }
    }
    else
    {
      connptr = accept_connection(p);
    }

    if (!connptr)
    {
      Warn("invalid or expired connection, ignoring");
      return;
    }

    handle_conn_packet(*connptr, p);
  }

  std::shared_ptr<Connection>
  Server::accept_connection(const Packet& p)
  {
    Debug("Accepting new connection");
    // This is a new incoming connection
    ngtcp2_pkt_hd hd;
    auto rv = ngtcp2_accept(&hd, u8data(p.data), p.data.size());

    if (rv == -1)
    {  // Invalid packet
      Warn("Invalid packet received, length=", p.data.size());
#ifndef NDEBUG
      Debug("packet body:");
      for (size_t i = 0; i < p.data.size(); i += 50)
        Debug("  ", oxenmq::to_hex(p.data.substr(i, 50)));
#endif
      return nullptr;
    }

    if (rv == 1)
    {  // Invalid/unexpected version, send a version negotiation
      Debug("Invalid/unsupported version; sending version negotiation");
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
      Warn("Received 0-RTT packet, which shouldn't happen in our implementation; dropping");
      return nullptr;
    }
    else if (hd.type == NGTCP2_PKT_INITIAL && hd.token.len)
    {
      // This is a normal QUIC thing, but we don't do it:
      Warn("Unexpected token in initial packet");
    }

    // create and store Connection
    for (;;)
    {
      if (auto [it, ins] = conns.emplace(ConnectionID::random(), primary_conn_ptr{}); ins)
      {
        auto connptr = std::make_shared<Connection>(*this, it->first, hd, p.path);
        it->second = connptr;
        Debug("Created local Connection ", it->first, " for incoming connection");
        return connptr;
      }
    }
  }

}  // namespace llarp::quic
