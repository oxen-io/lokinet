#include "endpoint.hpp"
#include "client.hpp"
#include "log.hpp"
#include "server.hpp"

#include <iostream>
#include <variant>

#include <oxenmq/hex.h>
#include <oxenmq/variant.h>

#include <uvw/timer.h>

#include <sodium/crypto_generichash.h>

// DEBUG:
extern "C"
{
#include "../ngtcp2_conn.h"
}

namespace llarp::quic
{
  Endpoint::Endpoint(std::optional<Address> addr, std::shared_ptr<uvw::Loop> loop_)
      : loop{std::move(loop_)}
  {
    random_bytes(static_secret.data(), static_secret.size(), rng);

    // Create and bind the UDP socket. We can't use libuv's UDP socket here because it doesn't
    // give us the ability to set up the ECN field as QUIC requires.
    auto fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (fd == -1)
      throw std::runtime_error{"Failed to open socket: "s + strerror(errno)};

    if (addr)
    {
      assert(addr->sockaddr_size() == sizeof(sockaddr_in));  // FIXME: IPv4-only for now
      auto rv = bind(fd, *addr, addr->sockaddr_size());
      if (rv == -1)
        throw std::runtime_error{
            "Failed to bind UDP socket to " + addr->to_string() + ": " + strerror(errno)};
    }

    // Get our address via the socket in case `addr` is using anyaddr/anyport.
    sockaddr_any sa;
    socklen_t salen = sizeof(sa);
    // FIXME: if I didn't call bind above then do I need to call bind() before this (with
    // anyaddr/anyport)?
    getsockname(fd, &sa.sa, &salen);
    assert(salen == sizeof(sockaddr_in));  // FIXME: IPv4-only for now
    local = {&sa, salen};
    Debug("Bound to ", local, addr ? "" : " (auto-selected)");

    // Set up the socket to provide us with incoming ECN (IP_TOS) info
    // NB: This is for IPv4; on AF_INET6 this would be IPPROTO_IPV6, IPV6_RECVTCLASS
    if (uint8_t want_tos = 1;
        - 1
        == setsockopt(
            fd, IPPROTO_IP, IP_RECVTOS, &want_tos, static_cast<socklen_t>(sizeof(want_tos))))
      throw std::runtime_error{"Failed to set ECN on socket: "s + strerror(errno)};

    // Wire up our recv buffer structures into what recvmmsg() wants
    buf.resize(max_buf_size * msgs.size());
    for (size_t i = 0; i < msgs.size(); i++)
    {
      auto& iov = msgs_iov[i];
      iov.iov_base = buf.data() + max_buf_size * i;
      iov.iov_len = max_buf_size;
#ifdef LOKINET_HAVE_RECVMMSG
      auto& mh = msgs[i].msg_hdr;
#else
      auto& mh = msgs[i];
#endif
      mh.msg_name = &msgs_addr[i];
      mh.msg_namelen = sizeof(msgs_addr[i]);
      mh.msg_iov = &iov;
      mh.msg_iovlen = 1;
      mh.msg_control = msgs_cmsg[i].data();
      mh.msg_controllen = msgs_cmsg[i].size();
    }

    // Let uv do its stuff
    poll = loop->resource<uvw::PollHandle>(fd);
    poll->on<uvw::PollEvent>([this](const auto&, auto&) { on_readable(); });
    poll->start(uvw::PollHandle::Event::READABLE);

    // Set up a callback every 250ms to clean up stale sockets, etc.
    expiry_timer = loop->resource<uvw::TimerHandle>();
    expiry_timer->on<uvw::TimerEvent>([this](const auto&, auto&) { check_timeouts(); });
    expiry_timer->start(250ms, 250ms);

    Debug("Created endpoint");
  }

  Endpoint::~Endpoint()
  {
    if (poll)
      poll->close();
    if (expiry_timer)
      expiry_timer->close();
  }

  int
  Endpoint::socket_fd() const
  {
    return poll->fd();
  }

  void
  Endpoint::on_readable()
  {
    Debug("poll callback on readable");

#ifdef LOKINET_HAVE_RECVMMSG
    // NB: recvmmsg is linux-specific but ought to offer some performance benefits
    int n_msg = recvmmsg(socket_fd(), msgs.data(), msgs.size(), 0, nullptr);
    if (n_msg == -1)
    {
      if (errno != EAGAIN && errno != ENOTCONN)
        Warn("Error recv'ing from ", local.to_string(), ": ", strerror(errno));
      return;
    }

    Debug("Recv'd ", n_msg, " messages");
    for (int i = 0; i < n_msg; i++)
    {
      auto& [msg_hdr, msg_len] = msgs[i];
      bstring_view data{buf.data() + i * max_buf_size, msg_len};
#else
    for (size_t i = 0; i < N_msgs; i++)
    {
      auto& msg_hdr = msgs[0];
      auto n_bytes = recvmsg(socket_fd(), &msg_hdr, 0);
      if (n_bytes == -1 && errno != EAGAIN && errno != ENOTCONN)
        Warn("Error recv'ing from ", local.to_string(), ": ", strerror(errno));
      if (n_bytes <= 0)
        return;
      auto msg_len = static_cast<unsigned int>(n_bytes);
      bstring_view data{buf.data(), msg_len};
#endif

      Debug(
          "header [",
          msg_hdr.msg_namelen,
          "]: ",
          buffer_printer{reinterpret_cast<char*>(msg_hdr.msg_name), msg_hdr.msg_namelen});

      if (!msg_hdr.msg_name || msg_hdr.msg_namelen != sizeof(sockaddr_in))
      {  // FIXME: IPv6 support?
        Warn("Invalid/unknown source address, dropping packet");
        continue;
      }

      Packet pkt{
          Path{local, reinterpret_cast<const sockaddr_any*>(msg_hdr.msg_name), msg_hdr.msg_namelen},
          data,
          ngtcp2_pkt_info{.ecn = 0}};

      // Go look for the ECN header field on the incoming packet
      for (auto cmsg = CMSG_FIRSTHDR(&msg_hdr); cmsg; cmsg = CMSG_NXTHDR(&msg_hdr, cmsg))
      {
        // IPv4; for IPv6 these would be IPPROTO_IPV6 and IPV6_TCLASS
        if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_TOS && cmsg->cmsg_len)
        {
          pkt.info.ecn = *reinterpret_cast<uint8_t*>(CMSG_DATA(cmsg));
        }
      }

      Debug(
          i,
          "[",
          pkt.path,
          ",ecn=0x",
          std::hex,
          +pkt.info.ecn,
          std::dec,
          "]: received ",
          msg_len,
          " bytes");

      handle_packet(pkt);

      Debug("Done handling packet");

#ifdef LOKINET_HAVE_RECVMMSG  // Help editor's { } matching:
    }
#else
    }
#endif
  }

  std::optional<ConnectionID>
  Endpoint::handle_packet_init(const Packet& p)
  {
    version_info vi;
    auto rv = ngtcp2_pkt_decode_version_cid(
        &vi.version,
        &vi.dcid,
        &vi.dcid_len,
        &vi.scid,
        &vi.scid_len,
        u8data(p.data),
        p.data.size(),
        NGTCP2_MAX_CIDLEN);
    if (rv == 1)
    {  // 1 means Version Negotiation should be sent and otherwise the packet should be ignored
      send_version_negotiation(vi, p.path.remote);
      return std::nullopt;
    }
    else if (rv != 0)
    {
      Warn("QUIC packet header decode failed: ", ngtcp2_strerror(rv));
      return std::nullopt;
    }

    if (vi.dcid_len > ConnectionID::max_size())
    {
      Warn("Internal error: destination ID is longer than should be allowed");
      return std::nullopt;
    }

    return std::make_optional<ConnectionID>(vi.dcid, vi.dcid_len);
  }
  void
  Endpoint::handle_conn_packet(Connection& conn, const Packet& p)
  {
    if (ngtcp2_conn_is_in_closing_period(conn))
    {
      Debug("Connection is in closing period, dropping");
      close_connection(conn);
      return;
    }
    if (conn.draining)
    {
      Debug("Connection is draining, dropping");
      // "draining" state means we received a connection close and we're keeping the
      // connection alive just to catch (and discard) straggling packets that arrive
      // out of order w.r.t to connection close.
      return;
    }

    if (auto result = read_packet(p, conn); !result)
    {
      Warn("Read packet failed! ", ngtcp2_strerror(result.error_code));
    }

    // FIXME - reset idle timer?
    Debug("Done with incoming packet");
  }

  io_result
  Endpoint::read_packet(const Packet& p, Connection& conn)
  {
    Debug("Reading packet from ", p.path);
    Debug("Conn state before reading: ", conn.conn->state);
    auto rv =
        ngtcp2_conn_read_pkt(conn, p.path, &p.info, u8data(p.data), p.data.size(), get_timestamp());
    Debug("Conn state after reading: ", conn.conn->state);

    if (rv == 0)
      conn.io_ready();
    else
      Warn("read pkt error: ", ngtcp2_strerror(rv));

    if (rv == NGTCP2_ERR_DRAINING)
      start_draining(conn);
    else if (rv == NGTCP2_ERR_DROP_CONN)
      delete_conn(conn.base_cid);

    return {rv};
  }

  void
  Endpoint::update_ecn(uint32_t ecn)
  {
    assert(ecn <= std::numeric_limits<uint8_t>::max());
    if (ecn_curr != ecn)
    {
      if (-1
          == setsockopt(socket_fd(), IPPROTO_IP, IP_TOS, &ecn, static_cast<socklen_t>(sizeof(ecn))))
        Warn("setsockopt failed to set IP_TOS: ", strerror(errno));

      // IPv6 version:
      // int tclass = this->ecn;
      // setsockopt(socket_fd(), IPPROTO_IPV6, IPV6_TCLASS, &tclass,
      // static_cast<socklen_t>(sizeof(tclass)));

      ecn_curr = ecn;
    }
  }

  io_result
  Endpoint::send_packet(const Address& to, bstring_view data, uint32_t ecn)
  {
    iovec msg_iov;
    msg_iov.iov_base = const_cast<std::byte*>(data.data());
    msg_iov.iov_len = data.size();

    msghdr msg{};
    msg.msg_name = &const_cast<sockaddr&>(reinterpret_cast<const sockaddr&>(to));
    msg.msg_namelen = sizeof(sockaddr_in);
    msg.msg_iov = &msg_iov;
    msg.msg_iovlen = 1;

    auto fd = socket_fd();

    update_ecn(ecn);
    ssize_t nwrite = 0;
    do
    {
      nwrite = sendmsg(fd, &msg, 0);
    } while (nwrite == -1 && errno == EINTR);

    if (nwrite == -1)
    {
      Warn("sendmsg failed: ", strerror(errno));
      return {errno};
    }

    Debug(
        "[",
        to.to_string(),
        ",ecn=0x",
        std::hex,
        +ecn_curr,
        std::dec,
        "]: sent ",
        nwrite,
        " bytes");
    return {};
  }

  void
  Endpoint::send_version_negotiation(const version_info& vi, const Address& source)
  {
    std::array<std::byte, NGTCP2_MAX_PKTLEN_IPV4> buf;
    std::array<uint32_t, NGTCP2_PROTO_VER_MAX - NGTCP2_PROTO_VER_MIN + 2> versions;
    std::iota(versions.begin() + 1, versions.end(), NGTCP2_PROTO_VER_MIN);
    // we're supposed to send some 0x?a?a?a?a version to trigger version negotiation
    versions[0] = 0x1a2a3a4au;

    auto nwrote = ngtcp2_pkt_write_version_negotiation(
        u8data(buf),
        buf.size(),
        std::uniform_int_distribution<uint8_t>{0, 255}(rng),
        vi.dcid,
        vi.dcid_len,
        vi.scid,
        vi.scid_len,
        versions.data(),
        versions.size());
    if (nwrote < 0)
      Warn("Failed to construct version negotiation packet: ", ngtcp2_strerror(nwrote));
    if (nwrote <= 0)
      return;

    send_packet(source, bstring_view{buf.data(), static_cast<size_t>(nwrote)}, 0);
  }

  void
  Endpoint::close_connection(Connection& conn, uint64_t code, bool application)
  {
    Debug("Closing connection ", conn.base_cid);
    if (!conn.closing)
    {
      conn.conn_buffer.resize(max_pkt_size_v4);
      Path path;
      ngtcp2_pkt_info pi;

      auto write_close_func =
          application ? ngtcp2_conn_write_application_close : ngtcp2_conn_write_connection_close;
      auto written = write_close_func(
          conn,
          path,
          &pi,
          u8data(conn.conn_buffer),
          conn.conn_buffer.size(),
          code,
          get_timestamp());
      if (written <= 0)
      {
        Warn(
            "Failed to write connection close packet: ",
            written < 0 ? ngtcp2_strerror(written) : "unknown error: closing is 0 bytes??");
        return;
      }
      assert(written <= (long)conn.conn_buffer.size());
      conn.conn_buffer.resize(written);
      conn.closing = true;

      // FIXME: ipv6
      assert(path.local.sockaddr_size() == sizeof(sockaddr_in));
      assert(path.remote.sockaddr_size() == sizeof(sockaddr_in));

      conn.path = path;
    }
    assert(conn.closing && !conn.conn_buffer.empty());

    if (auto sent = send_packet(conn.path.remote, conn.conn_buffer, 0); !sent)
    {
      Warn(
          "Failed to send packet: ",
          strerror(sent.error_code),
          "; removing connection ",
          conn.base_cid);
      delete_conn(conn.base_cid);
      return;
    }
  }

  /// Puts a connection into draining mode (i.e. after getting a connection close).  This will
  /// keep the connection registered for the recommended 3*Probe Timeout, during which we drop
  /// packets that use the connection id and after which we will forget about it.
  void
  Endpoint::start_draining(Connection& conn)
  {
    if (conn.draining)
      return;
    Debug("Putting ", conn.base_cid, " into draining mode");
    conn.draining = true;
    // Recommended draining time is 3*Probe Timeout
    draining.emplace(conn.base_cid, get_time() + ngtcp2_conn_get_pto(conn) * 3 * 1ns);
  }

  void
  Endpoint::check_timeouts()
  {
    auto now = get_time();
    uint64_t now_ts = get_timestamp(now);

    // Destroy any connections that are finished draining
    bool cleanup = false;
    while (!draining.empty() && draining.front().second < now)
    {
      if (auto it = conns.find(draining.front().first); it != conns.end())
      {
        if (std::holds_alternative<primary_conn_ptr>(it->second))
          cleanup = true;
        Debug("Deleting connection ", it->first);
        conns.erase(it);
      }
      draining.pop();
    }
    if (cleanup)
      clean_alias_conns();

    for (auto it = conns.begin(); it != conns.end(); ++it)
    {
      if (auto* conn_ptr = std::get_if<primary_conn_ptr>(&it->second))
      {
        Connection& conn = **conn_ptr;
        auto exp = ngtcp2_conn_get_idle_expiry(conn);
        if (exp >= now_ts || conn.draining)
          continue;
        start_draining(conn);
      }
    }
  }

  std::pair<std::shared_ptr<Connection>, bool>
  Endpoint::get_conn(const ConnectionID& cid)
  {
    if (auto it = conns.find(cid); it != conns.end())
    {
      if (auto* wptr = std::get_if<alias_conn_ptr>(&it->second))
        return {wptr->lock(), true};
      return {var::get<primary_conn_ptr>(it->second), false};
    }
    return {nullptr, false};
  }

  bool
  Endpoint::delete_conn(const ConnectionID& cid)
  {
    auto it = conns.find(cid);
    if (it == conns.end())
    {
      Debug("Cannot delete connection ", cid, ": cid not found");
      return false;
    }

    bool primary = std::holds_alternative<primary_conn_ptr>(it->second);
    Debug("Deleting ", primary ? "primary" : "alias", " connection ", cid);
    conns.erase(it);
    if (primary)
      clean_alias_conns();
    return true;
  }

  void
  Endpoint::clean_alias_conns()
  {
    for (auto it = conns.begin(); it != conns.end();)
    {
      if (auto* conn_wptr = std::get_if<alias_conn_ptr>(&it->second);
          conn_wptr && conn_wptr->expired())
        it = conns.erase(it);
      else
        ++it;
    }
  }

  ConnectionID
  Endpoint::add_connection_id(Connection& conn, size_t cid_length)
  {
    ConnectionID cid;
    for (bool inserted = false; !inserted;)
    {
      cid = ConnectionID::random(rng, cid_length);
      inserted = conns.emplace(cid, conn.weak_from_this()).second;
    }
    Debug("Created cid ", cid, " alias for ", conn.base_cid);
    return cid;
  }

  void
  Endpoint::make_stateless_reset_token(const ConnectionID& cid, unsigned char* dest)
  {
    crypto_generichash_state state;
    crypto_generichash_init(&state, nullptr, 0, NGTCP2_STATELESS_RESET_TOKENLEN);
    crypto_generichash_update(&state, u8data(static_secret), static_secret.size());
    crypto_generichash_update(&state, cid.data, cid.datalen);
    crypto_generichash_final(&state, dest, NGTCP2_STATELESS_RESET_TOKENLEN);
  }

}  // namespace llarp::quic
