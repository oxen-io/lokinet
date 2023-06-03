#include "connection.hpp"
#include "client.hpp"
#include "external/ngtcp2/lib/ngtcp2_conn.h"
#include "llarp/crypto/crypto.hpp"
#include "llarp/quic/stream.hpp"
#include "oxen/log.hpp"
#include "server.hpp"
#include <ngtcp2/ngtcp2.h>
#include <external/ngtcp2/crypto/includes/ngtcp2/ngtcp2_crypto.h>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <llarp/util/logging.hpp>
#include <llarp/util/logging/buffer.hpp>

#include <cassert>
#include <charconv>
#include <cstring>
#include <iostream>

#include <uvw/async.h>
#include <uvw/poll.h>
#include <uvw/timer.h>

#include <iterator>
#include <oxenc/hex.h>
#include <oxenc/bt_serialize.h>

extern "C"
{
#include <sodium/randombytes.h>
}

namespace llarp::quic
{
  static auto logcat = log::Cat("quic");

  ConnectionID::ConnectionID(const uint8_t* cid, size_t length)
  {
    assert(length <= max_size());
    datalen = length;
    std::memmove(data, cid, datalen);
  }

  std::string
  ConnectionID::ToString() const
  {
    return oxenc::to_hex(data, data + datalen);
  }

  ConnectionID
  ConnectionID::random(size_t size)
  {
    ConnectionID r;
    r.datalen = std::min(size, ConnectionID::max_size());
    randombytes_buf(r.data, r.datalen);
    return r;
  }

  namespace
  {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

    constexpr int CALLBACK_FAIL = NGTCP2_ERR_CALLBACK_FAILURE;

    int
    client_initial(ngtcp2_conn* conn_, void* user_data)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

      // Initialization the connection and send our transport parameters to the server.  This will
      // put the connection into NGTCP2_CS_CLIENT_WAIT_HANDSHAKE state.
      return static_cast<Connection*>(user_data)->init_client();
    }
    int
    recv_client_initial(ngtcp2_conn* conn_, const ngtcp2_cid* dcid, void* user_data)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

      // New incoming connection from a client: our server connection starts out here in state
      // NGTCP2_CS_SERVER_INITIAL, but we should immediately get into recv_crypto_data because the
      // initial client packet should contain the client's transport parameters.

      auto& conn = *static_cast<Connection*>(user_data);
      assert(conn_ == conn.conn.get());

      if (0 != conn.setup_server_crypto_initial())
        return CALLBACK_FAIL;

      return 0;
    }
    int
    recv_crypto_data(
        ngtcp2_conn* conn_,
        ngtcp2_crypto_level crypto_level,
        uint64_t offset,
        const uint8_t* rawdata,
        size_t rawdatalen,
        void* user_data)
    {
      std::basic_string_view data{rawdata, rawdatalen};
      log::trace(logcat, "Receiving crypto data @ level {} {}", crypto_level, buffer_printer{data});

      auto& conn = *static_cast<Connection*>(user_data);
      switch (crypto_level)
      {
        case NGTCP2_CRYPTO_LEVEL_EARLY:
          // We don't currently use or support 0rtt
          log::warning(logcat, "Invalid EARLY crypto level");
          return CALLBACK_FAIL;

        case NGTCP2_CRYPTO_LEVEL_INITIAL:
          log::debug(logcat, "Entering case NGTCP2_CRYPTO_LEVEL_INITIAL");
          // "Initial" level means we are still handshaking; if we are server then we receive
          // the client's transport params (sent in client_initial, above) and blast ours
          // back.  If we are a client then getting here means we received a response from the
          // server, which is that returned server transport params.

          if (auto rv = conn.recv_initial_crypto(data); rv != 0)
          {
            log::debug(
                logcat, "conn.recv_initial_crypto() returned error: {}", ngtcp2_strerror(rv));
            return rv;
          }

          if (ngtcp2_conn_is_server(conn))
          {
            if (auto rv = conn.send_magic(NGTCP2_CRYPTO_LEVEL_INITIAL); rv != 0)
            {
              log::debug(logcat, "conn.send_magic() returned error: {}", ngtcp2_strerror(rv));
              return rv;
            }
            if (auto rv = conn.send_transport_params(NGTCP2_CRYPTO_LEVEL_HANDSHAKE); rv != 0)
            {
              log::debug(
                  logcat, "conn.send_transport_params() returned error: {}", ngtcp2_strerror(rv));
              return rv;
            }
          }

          break;

        case NGTCP2_CRYPTO_LEVEL_HANDSHAKE:
          log::debug(logcat, "Entering case NGTCP2_CRYPTO_LEVEL_HANDSHAKE");
          if (!ngtcp2_conn_is_server(conn))
          {
            if (auto rv = conn.recv_transport_params(data); rv != 0)
            {
              log::debug(
                  logcat, "conn.recv_transport_params() returned error: {}", ngtcp2_strerror(rv));
              return rv;
            }
            // At this stage of the protocol with TLS the client sends back TLS info so that
            // the server can install our rx key; we have to send *something* back to invoke
            // the server's HANDSHAKE callback (so that it knows handshake is complete) so
            // send the magic again.
            if (auto rv = conn.send_magic(NGTCP2_CRYPTO_LEVEL_HANDSHAKE); rv != 0)
            {
              log::debug(logcat, "conn.send_magic() returned error: {}", ngtcp2_strerror(rv));
              return rv;
            }
          }
          else
          {
            // Check that we received the above as expected
            if (data != handshake_magic)
            {
              log::debug(
                  logcat,
                  "Invalid handshake crypto frame from client: did not find expected magic");
              return NGTCP2_ERR_CALLBACK_FAILURE;
            }
          }

          if (auto rv = conn.complete_handshake(); rv != 0)
            return rv;

          break;

        case NGTCP2_CRYPTO_LEVEL_APPLICATION:
          log::debug(logcat, "Entering case NGTCP2_CRYPTO_LEVEL_APPLICATION");
          // if (!conn.init_tx_key())
          //    return CALLBACK_FAIL;
          break;

        default:
          log::warning(logcat, "Unhandled crypto_level {}", crypto_level);
          return CALLBACK_FAIL;
      }
      conn.io_ready();
      return 0;
    }

    int
    version_negotiation(
        ngtcp2_conn* conn, uint32_t version, const ngtcp2_cid* client_dcid, void* user_data)
    {
      log::info(logcat, "{} called", __PRETTY_FUNCTION__);
      return 0;
    }

    int
    handshake_completed(ngtcp2_conn* _conn, void* user_data)
    {
      log::info(logcat, "{} called", __PRETTY_FUNCTION__);

      auto& conn = *static_cast<Connection*>(user_data);

      if (auto rv = conn.complete_handshake(); rv != 0)
        return CALLBACK_FAIL;

      return 0;
    }

    int
    encrypt(
        uint8_t* dest,
        const ngtcp2_crypto_aead* aead,
        const ngtcp2_crypto_aead_ctx* aead_ctx,
        const uint8_t* plaintext,
        size_t plaintextlen,
        const uint8_t* nonce,
        size_t noncelen,
        const uint8_t* ad,
        size_t adlen)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
      log::trace(logcat, "Lengths: {}+{}+{}", plaintextlen, noncelen, adlen);
      if (dest != plaintext)
        std::memmove(dest, plaintext, plaintextlen);
      return 0;
    }
    int
    decrypt(
        uint8_t* dest,
        const ngtcp2_crypto_aead* aead,
        const ngtcp2_crypto_aead_ctx* aead_ctx,
        const uint8_t* ciphertext,
        size_t ciphertextlen,
        const uint8_t* nonce,
        size_t noncelen,
        const uint8_t* ad,
        size_t adlen)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
      log::trace(logcat, "Lengths: {}+{}+{}", ciphertextlen, noncelen, adlen);
      if (dest != ciphertext)
        std::memmove(dest, ciphertext, ciphertextlen);
      return 0;
    }
    int
    hp_mask(
        uint8_t* dest,
        const ngtcp2_crypto_cipher* hp,
        const ngtcp2_crypto_cipher_ctx* hp_ctx,
        const uint8_t* sample)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
      memset(dest, 0, NGTCP2_HP_MASKLEN);
      return 0;
    }
    int
    recv_stream_data(
        ngtcp2_conn* conn,
        uint32_t flags,
        int64_t stream_id,
        uint64_t offset,
        const uint8_t* data,
        size_t datalen,
        void* user_data,
        void* stream_user_data)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
      return static_cast<Connection*>(user_data)->stream_receive(
          {stream_id},
          {reinterpret_cast<const std::byte*>(data), datalen},
          flags & NGTCP2_STREAM_DATA_FLAG_FIN);
    }

    int
    acked_stream_data_offset(
        ngtcp2_conn* conn_,
        int64_t stream_id,
        uint64_t offset,
        uint64_t datalen,
        void* user_data,
        void* stream_user_data)
    {
      log::debug(logcat, "{} called", __PRETTY_FUNCTION__);
      log::debug(logcat, "Ack [{},{})", offset, offset + datalen);
      return static_cast<Connection*>(user_data)->stream_ack({stream_id}, datalen);
    }

    int
    stream_open(ngtcp2_conn* conn, int64_t stream_id, void* user_data)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
      return static_cast<Connection*>(user_data)->stream_opened({stream_id});
    }

    int
    stream_close_cb(
        ngtcp2_conn* conn,
        uint32_t flags,
        int64_t stream_id,
        uint64_t app_error_code,
        void* user_data,
        void* stream_user_data)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
      static_cast<Connection*>(user_data)->stream_closed({stream_id}, app_error_code);
      return 0;
    }
    int
    stream_reset_cb(
        ngtcp2_conn* conn,
        int64_t stream_id,
        uint64_t final_size,
        uint64_t app_error_code,
        void* user_data,
        void* stream_user_data)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
      static_cast<Connection*>(user_data)->stream_closed({stream_id}, app_error_code);
      return 0;
    }

    // (client only)
    int
    handshake_confirmed(ngtcp2_conn* conn, void* user_data)
    {
      log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

      auto& client = *static_cast<Connection*>(user_data);

      if (auto rv = client.get_handshake_confirmed(); rv != 0)
        return rv;

      return 0;
    }

    // (client only)
    int
    recv_retry(ngtcp2_conn* conn, const ngtcp2_pkt_hd* hd, void* user_data)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
      log::error(logcat, "FIXME UNIMPLEMENTED {}", __PRETTY_FUNCTION__);
      // FIXME
      return 0;
    }
    int
    extend_max_local_streams_bidi(ngtcp2_conn* conn_, uint64_t max_streams, void* user_data)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
      auto& conn = *static_cast<Connection*>(user_data);
      if (conn.on_stream_available)
        if (uint64_t left = ngtcp2_conn_get_streams_bidi_left(conn); left > 0)
          conn.on_stream_available(conn);

      return 0;
    }

    void
    rand(uint8_t* dest, size_t destlen, const ngtcp2_rand_ctx* rand_ctx)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
      randombytes_buf(dest, destlen);
    }

    int
    get_new_connection_id(
        ngtcp2_conn* conn_, ngtcp2_cid* cid_, uint8_t* token, size_t cidlen, void* user_data)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

      auto& conn = *static_cast<Connection*>(user_data);
      auto cid = conn.make_alias_id(cidlen);
      assert(cid.datalen == cidlen);
      *cid_ = cid;

      conn.endpoint.make_stateless_reset_token(cid, token);
      log::debug(
          logcat,
          "make stateless reset token {}",
          oxenc::to_hex(token, token + NGTCP2_STATELESS_RESET_TOKENLEN));

      return 0;
    }
    int
    remove_connection_id(ngtcp2_conn* conn, const ngtcp2_cid* cid, void* user_data)
    {
      log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
      log::error(logcat, "FIXME UNIMPLEMENTED {}", __PRETTY_FUNCTION__);
      // FIXME
      return 0;
    }
    int
    update_key(
        ngtcp2_conn* conn,
        uint8_t* rx_secret,
        uint8_t* tx_secret,
        ngtcp2_crypto_aead_ctx* rx_aead_ctx,
        uint8_t* rx_iv,
        ngtcp2_crypto_aead_ctx* tx_aead_ctx,
        uint8_t* tx_iv,
        const uint8_t* current_rx_secret,
        const uint8_t* current_tx_secret,
        size_t secretlen,
        void* user_data)
    {
      // This is a no-op since we don't encrypt anything in the first place
      return 0;
    }
#pragma GCC diagnostic pop
  }  // namespace

#ifndef NDEBUG
  extern "C" inline void
  ngtcp_trace_logger([[maybe_unused]] void* user_data, const char* fmt, ...)
  {
    std::array<char, 2048> buf{};
    va_list ap;
    va_start(ap, fmt);
    if (vsnprintf(buf.data(), buf.size(), fmt, ap) >= 0)
      log::debug(logcat, "{}", buf.data());
    va_end(ap);
  }
#endif

  io_result
  Connection::send()
  {
    assert(send_buffer_size <= send_buffer.size());
    io_result rv{};
    bstring_view send_data{send_buffer.data(), send_buffer_size};

    if (!send_data.empty())
    {
      rv = endpoint.send_packet(path.remote, send_data, send_pkt_info.ecn);
    }
    return rv;
  }

  std::tuple<ngtcp2_settings, ngtcp2_transport_params, ngtcp2_callbacks>
  Connection::init()
  {
    auto loop = endpoint.get_loop();
    io_trigger = loop->resource<uvw::AsyncHandle>();
    io_trigger->on<uvw::AsyncEvent>([this](auto&, auto&) { on_io_ready(); });

    retransmit_timer = loop->resource<uvw::TimerHandle>();
    retransmit_timer->on<uvw::TimerEvent>([this](auto&, auto&) {
      log::debug(logcat, "Retransmit timer fired!");
      if (auto rv = ngtcp2_conn_handle_expiry(conn.get(), get_timestamp()); rv != 0)
      {
        log::warning(
            logcat, "expiry handler invocation returned an error: {}", ngtcp2_strerror(rv));
        endpoint.close_connection(*this, rv);
      }
      else
      {
        on_io_ready();
      }
    });
    retransmit_timer->start(0ms, 0ms);

    auto result = std::tuple<ngtcp2_settings, ngtcp2_transport_params, ngtcp2_callbacks>{};
    auto& [settings, tparams, cb] = result;
    cb.recv_crypto_data = recv_crypto_data;
    cb.handshake_completed = handshake_completed;
    cb.encrypt = encrypt;
    cb.decrypt = decrypt;
    cb.hp_mask = hp_mask;
    cb.recv_stream_data = recv_stream_data;
    cb.acked_stream_data_offset = acked_stream_data_offset;
    cb.stream_open = stream_open;
    cb.stream_close = stream_close_cb;
    cb.stream_reset = stream_reset_cb;
    cb.delete_crypto_aead_ctx = [](auto, auto, auto) {};
    cb.delete_crypto_cipher_ctx = [](auto, auto, auto) {};
    cb.get_path_challenge_data = [](ngtcp2_conn*, uint8_t* data, void*) {
      llarp::CryptoManager::instance()->randbytes(data, NGTCP2_PATH_CHALLENGE_DATALEN);
      return 0;
    };
    cb.version_negotiation = version_negotiation;
    cb.extend_max_local_streams_bidi = extend_max_local_streams_bidi;
    cb.rand = rand;
    cb.get_new_connection_id = get_new_connection_id;
    cb.remove_connection_id = remove_connection_id;
    cb.update_key = update_key;

    ngtcp2_settings_default(&settings);
#ifndef NDEBUG
    settings.log_printf = ngtcp_trace_logger;
#endif

    settings.initial_ts = get_timestamp();
    // FIXME: IPv6
    settings.max_tx_udp_payload_size = Endpoint::max_pkt_size_v4;
    settings.cc_algo = NGTCP2_CC_ALGO_CUBIC;
    // settings.initial_rtt = std::chrono::nanoseconds(333ms).count(); // NGTCP2's default is 333ms

    ngtcp2_transport_params_default(&tparams);

    // Connection level flow control window:
    tparams.initial_max_data = CONNECTION_BUFFER;
    // Max send buffer for a streams (local is for streams we initiate, remote is for replying on
    // streams they initiate to us):
    tparams.initial_max_stream_data_bidi_local = STREAM_BUFFER;
    tparams.initial_max_stream_data_bidi_remote = STREAM_BUFFER;
    // Max *concurrent* streams we support on a connection:
    tparams.initial_max_streams_bidi = STREAM_LIMIT;
    tparams.initial_max_streams_uni = 0;
    tparams.max_idle_timeout = std::chrono::nanoseconds(IDLE_TIMEOUT).count();
    tparams.active_connection_id_limit = 8;

    log::debug(logcat, "Done basic connection initialization");

    return result;
  }

  Connection::Connection(
      Server& s, const ConnectionID& base_cid_, ngtcp2_pkt_hd& header, const Path& path)
      : endpoint{s}, base_cid{base_cid_}, dest_cid{header.scid}, path{path}
  {
    auto [settings, tparams, cb] = init();

    cb.recv_client_initial = recv_client_initial;

    // ConnectionIDs are a little complicated:
    // - when a client creates a new connection to us, it creates a random source connection ID
    //   *and* a random destination connection id.  The server won't have that connection ID, of
    //   course, but we use it to recognize that we should try accepting it as a new connection.
    // - When we talk to the client we use the random source connection ID that it generated as our
    //   destination connection ID.
    // - We choose our own source ID, however: we *don't* use the random one the client picked for
    //   us.  Instead we generate a random one and sent it back as *our* source connection ID in the
    //   reply to the client.
    // - the client still needs to match up that reply with that request, and so we include the
    //   destination connection ID that the client generated for us in the transport parameters as
    //   the original_dcid: this lets the client match up the request, after which it can't promptly
    //   forget about it and start using the source CID that we gave it.
    //
    // So, in other words, the conversation goes like this:
    // - Client: [SCID:clientid, DCID:randomid, TRANSPORT_PARAMS]
    // - Server: [SCID:serverid, DCID:clientid TRANSPORT_PARAMS(origid=randomid)]
    //
    // - For the client, .base_cid={clientid} and .dest_cid={randomid} initially but gets updated to
    // .dest_cid={serverid} when we hear back from the server.
    // - For the server, .base_cid={serverid} and .dest_cid={clientid}

    tparams.original_dcid = header.dcid;

    log::debug(logcat, "original_dcid is now set to {}", ConnectionID(tparams.original_dcid));

    settings.token = header.token;

    // FIXME is this required?
    randombytes_buf(tparams.stateless_reset_token, sizeof(tparams.stateless_reset_token));
    tparams.stateless_reset_token_present = 1;

    ngtcp2_conn* connptr;
    log::debug(logcat, "server_new, path = {}", path);
    if (auto rv = ngtcp2_conn_server_new(
            &connptr,
            &dest_cid,
            &base_cid,
            path,
            header.version,
            &cb,
            &settings,
            &tparams,
            nullptr /*default mem allocator*/,
            this);
        rv != 0)
      throw std::runtime_error{"Failed to initialize server connection: "s + ngtcp2_strerror(rv)};
    conn.reset(connptr);

    log::debug(logcat, "Created new server Connection object {}", base_cid);
  }

  Connection::Connection(
      Client& c, const ConnectionID& scid, const Path& path, uint16_t tunnel_port)
      : tunnel_port{tunnel_port}
      , endpoint{c}
      , base_cid{scid}
      , dest_cid{ConnectionID::random()}
      , path{path}
  {
    auto [settings, tparams, cb] = init();

    assert(tunnel_port != 0);

    cb.client_initial = client_initial;
    cb.recv_retry = recv_retry;
    // cb.handshake_confirmed = handshake_confirmed;
    //  cb.extend_max_local_streams_bidi = extend_max_local_streams_bidi;
    //  cb.recv_new_token = recv_new_token;

    ngtcp2_conn* connptr;

    if (auto rv = ngtcp2_conn_client_new(
            &connptr,
            &dest_cid,
            &scid,
            path,
            NGTCP2_PROTO_VER_V1,
            &cb,
            &settings,
            &tparams,
            nullptr,
            this);
        rv != 0)
      throw std::runtime_error{"Failed to initialize client connection: "s + ngtcp2_strerror(rv)};
    conn.reset(connptr);

    log::debug(logcat, "Created new client Connection object {}", scid);
  }

  Connection::~Connection()
  {
    if (io_trigger)
      io_trigger->close();
    if (retransmit_timer)
    {
      retransmit_timer->stop();
      retransmit_timer->close();
    }
  }

  void
  Connection::io_ready()
  {
    io_trigger->send();
  }

  void
  Connection::on_io_ready()
  {
    log::debug(logcat, "{} called", __PRETTY_FUNCTION__);
    flush_streams();
    schedule_retransmit();
    log::debug(logcat, "{} finished", __PRETTY_FUNCTION__);
  }

  void
  Connection::flush_streams()
  {
    log::debug(logcat, "Connection::flush_streams()");
    // Maximum number of stream data packets to send out at once; if we reach this then we'll
    // schedule another event loop call of ourselves (so that we don't starve the loop).
    auto max_udp_payload_size = ngtcp2_conn_get_max_tx_udp_payload_size(conn.get());
    auto max_stream_packets = ngtcp2_conn_get_send_quantum(conn.get()) / max_udp_payload_size;
    ngtcp2_ssize ndatalen;
    uint16_t stream_packets = 0;
    uint32_t flags = NGTCP2_WRITE_STREAM_FLAG_MORE;
    uint64_t ts = get_timestamp();
    send_pkt_info = {};

    auto send_packet = [&](auto nwrite) -> int {
      send_buffer_size = nwrite;
      log::trace(logcat, "Sending {}B packet", send_buffer_size);

      auto sent = send();
      if (sent.blocked())
      {
        log::debug(logcat, "Packet send blocked, scheduling retransmit");
        ngtcp2_conn_update_pkt_tx_time(conn.get(), ts);
        schedule_retransmit();
        return 0;
      }

      send_buffer_size = 0;
      if (!sent)
      {
        log::warning(logcat, "I/O error while trying to send packet: {}", sent.str());
        // FIXME: disconnect?
        ngtcp2_conn_update_pkt_tx_time(conn.get(), ts);
        return 0;
      }
      log::debug(logcat, "packet away!");
      return 1;
    };

    std::list<Stream*> strs;
    for (auto& [stream_id, stream_ptr] : streams)
    {
      if (stream_ptr and not stream_ptr->sent_fin)
      {
        try
        {
          strs.push_back(stream_ptr.get());
        }
        catch (std::exception& e)
        {
          log::warning(logcat, "Exception caught: {}", e.what());
        }
      }
    }

    while (!strs.empty() && stream_packets < max_stream_packets)
    {
      for (auto it = strs.begin(); it != strs.end();)
      {
        log::debug(
            logcat,
            "Max stream packets: {}\nCurrent stream packets: {}",
            max_stream_packets,
            stream_packets);

        auto& stream = **it;
        auto bufs = stream.pending();

        if (bufs.empty())
        {
          log::debug(logcat, "Stream buffer empty, have you considered moving on");
        }

        std::vector<ngtcp2_vec> vecs;
        vecs.reserve(bufs.size());
        std::transform(bufs.begin(), bufs.end(), std::back_inserter(vecs), [](const auto& buf) {
          return ngtcp2_vec{const_cast<uint8_t*>(u8data(buf)), buf.size()};
        });

#ifndef NDEBUG
        {
          std::string buf_sizes;
          for (auto& b : bufs)
          {
            if (!buf_sizes.empty())
              buf_sizes += '+';
            buf_sizes += std::to_string(b.size());
          }
          log::debug(
              logcat, "Sending {} data for {}", buf_sizes.empty() ? "no" : buf_sizes, stream.id());
        }
#endif

        if (stream.is_closing && !stream.sent_fin && stream.unsent() == 0)
        {
          log::debug(logcat, "Sending FIN");
          flags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
          stream.sent_fin = true;
        }
        else if (stream.is_new)
        {
          stream.is_new = false;
        }

        auto nwrite = ngtcp2_conn_writev_stream(
            conn.get(),
            &path.path,
            &send_pkt_info,
            u8data(send_buffer),
            send_buffer.size(),
            &ndatalen,
            flags,
            stream.id().id,
            reinterpret_cast<const ngtcp2_vec*>(vecs.data()),
            vecs.size(),
            (!ts) ? get_timestamp() : ts);

        log::debug(
            logcat,
            "add_stream_data for stream {} returned [{},{}]",
            stream.id(),
            nwrite,
            ndatalen);

        if (nwrite < 0)
        {
          if (nwrite == -240)  // NGTCP2_ERR_WRITE_MORE
          {
            log::debug(
                logcat,
                "Consumed {} bytes from stream {} and have space left",
                ndatalen,
                stream.id());
            assert(ndatalen >= 0);
            stream.wrote(ndatalen);
            it = strs.erase(it);
            continue;
          }
          if (nwrite == NGTCP2_ERR_CLOSING)  // -230
          {
            log::debug(logcat, "Cannot write to {}: stream is closing", stream.id());
            it = strs.erase(it);
            continue;
          }
          if (nwrite == NGTCP2_ERR_STREAM_SHUT_WR)  // -230
          {
            log::debug(logcat, "Cannot add to stream {}: stream is shut, proceeding", stream.id());
            assert(ndatalen == -1);
            it = strs.erase(it);
            continue;
          }
          if (nwrite == NGTCP2_ERR_STREAM_DATA_BLOCKED)  // -210
          {
            log::debug(logcat, "Cannot add to stream {}: stream is blocked", stream.id());
            it = strs.erase(it);
            continue;
          }

          log::warning(logcat, "Error writing non-stream data: {}", ngtcp2_strerror(nwrite));
          break;
        }

        if (ndatalen >= 0)
        {
          log::debug(logcat, "consumed {} bytes from stream {}", ndatalen, stream.id());
          stream.wrote(ndatalen);
        }

        if (nwrite == 0)  //  we are congested
        {
          log::debug(logcat, "Done stream writing to {} (stream is congested)", stream.id());

          ngtcp2_conn_stat cstat;
          ngtcp2_conn_get_conn_stat(conn.get(), &cstat);
          log::debug(
              logcat,
              "Current unacked bytes in flight: {}, Congestion window: {}",
              cstat.bytes_in_flight,
              cstat.cwnd);
          ngtcp2_conn_update_pkt_tx_time(conn.get(), ts);
          //  we are congested, so clear pending streams to exit outer loop
          //  and enter next loop to flush unsent stuff
          strs.clear();
          break;
        }

        log::debug(logcat, "Sending stream data packet");
        if (!send_packet(nwrite))
          return;

        ngtcp2_conn_update_pkt_tx_time(conn.get(), ts);
        if (stream.unsent() == 0)
          it = strs.erase(it);
        else
          ++it;

        if (++stream_packets == max_stream_packets)
        {
          log::debug(logcat, "Max stream packets ({}) reached", max_stream_packets);
          ngtcp2_conn_update_pkt_tx_time(conn.get(), ts);
          return;
        }
      }
    }

    // Now try more with stream id -1 and no data: this takes care of things like initial handshake
    // packets, and also finishes off any partially-filled packet from above.
    for (;;)
    {
      log::debug(logcat, "Calling add_stream_data for empty stream");

      auto nwrite = ngtcp2_conn_writev_stream(
          conn.get(),
          &path.path,
          &send_pkt_info,
          u8data(send_buffer),
          send_buffer.size(),
          &ndatalen,
          flags,
          -1,
          nullptr,
          0,
          (!ts) ? get_timestamp() : ts);

      log::debug(logcat, "add_stream_data for non-stream returned [{},{}]", nwrite, ndatalen);
      assert(ndatalen <= 0);

      if (nwrite == 0)
      {
        log::debug(
            logcat, "Nothing else to write for non-stream data for now (or we are congested)");

        ngtcp2_conn_stat cstat;
        ngtcp2_conn_get_conn_stat(conn.get(), &cstat);
        log::debug(
            logcat,
            "Current unacked bytes in flight: {}, Congestion window: {}",
            cstat.bytes_in_flight,
            cstat.cwnd);

        break;
      }

      if (nwrite < 0)
      {
        if (nwrite == NGTCP2_ERR_WRITE_MORE)  // -240
        {
          log::debug(logcat, "Writing non-stream data frames, and have space left");
          ngtcp2_conn_update_pkt_tx_time(conn.get(), ts);
          continue;
        }
        if (nwrite == NGTCP2_ERR_CLOSING)  // -230
        {
          log::warning(logcat, "Error writing non-stream data: {}", ngtcp2_strerror(nwrite));
          break;
        }
        if (nwrite == NGTCP2_ERR_STREAM_DATA_BLOCKED)  // -210
        {
          log::debug(logcat, "cannot add to empty stream right now: stream is blocked");
          break;
        }

        log::warning(logcat, "Error writing non-stream data: {}", ngtcp2_strerror(nwrite));
        break;
      }

      log::debug(logcat, "Sending data packet with non-stream data frames");
      if (auto rv = send_packet(nwrite); rv != 0)
        return;
      ngtcp2_conn_update_pkt_tx_time(conn.get(), ts);
    }

    log::debug(logcat, "Exiting flush_streams()");
  }

  void
  Connection::schedule_retransmit()
  {
    auto exp = ngtcp2_conn_get_expiry(conn.get());
    auto expiry = std::chrono::nanoseconds{static_cast<std::chrono::nanoseconds::rep>(exp)};
    auto ngtcp2_expiry_delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        expiry - get_time().time_since_epoch());

    log::debug(logcat, "ngtcp2_conn_get_expiry: {} from now", ngtcp2_expiry_delta.count());

    if (exp == std::numeric_limits<decltype(exp)>::max())
    {
      log::debug(logcat, "no retransmit currently needed");
      retransmit_timer->stop();
      return;
    }

    auto expires_in = std::max(0ms, ngtcp2_expiry_delta);
    log::debug(logcat, "Next retransmit in {}ms", expires_in.count());
    retransmit_timer->stop();
    retransmit_timer->start(expires_in, 0ms);
  }

  int
  Connection::stream_opened(StreamID id)
  {
    log::debug(logcat, "New stream {}", id);
    auto* serv = server();
    if (!serv)
    {
      log::warning(logcat, "We are a client, incoming streams are not accepted");
      return NGTCP2_ERR_CALLBACK_FAILURE;
    }

    std::shared_ptr<Stream> stream{new Stream{*this, id, endpoint.default_stream_buffer_size}};
    stream->stream_id = id;
    bool good = true;
    if (serv->stream_open_callback)
      good = serv->stream_open_callback(*stream, tunnel_port);
    if (!good)
    {
      log::debug(logcat, "stream_open_callback returned failure, dropping stream {}", id);
      ngtcp2_conn_shutdown_stream(conn.get(), id.id, 1);
      io_ready();
      return NGTCP2_ERR_CALLBACK_FAILURE;
    }

    [[maybe_unused]] auto [it, ins] = streams.emplace(id, std::move(stream));
    assert(ins);
    log::debug(logcat, "Created new incoming stream {}", id);
    return 0;
  }

  int
  Connection::stream_receive(StreamID id, const bstring_view data, bool fin)
  {
    auto str = get_stream(id);
    if (!str->data_callback)
      log::debug(
          logcat,
          "Dropping incoming data on stream {}: stream has no data callback set",
          str->id());
    else
    {
      bool good = false;
      try
      {
        str->data_callback(*str, data);
        good = true;
      }
      catch (const std::exception& e)
      {
        log::warning(
            logcat,
            "Stream {} data callback raised exception ({}); closing stream with app code {}",
            str->id(),
            e.what(),
            STREAM_ERROR_EXCEPTION);
      }
      catch (...)
      {
        log::warning(
            logcat,
            "Stream {} data callback raised an unknown exception; closing stream with app code {}",
            str->id(),
            STREAM_ERROR_EXCEPTION);
      }
      if (!good)
      {
        str->close(STREAM_ERROR_EXCEPTION);
        return NGTCP2_ERR_CALLBACK_FAILURE;
      }
    }
    if (fin)
    {
      log::trace(logcat, "Stream {} closed by remote", str->id());
      // Don't cleanup here; stream_closed is going to be called right away to deal with that
    }
    else
    {
      ngtcp2_conn_extend_max_stream_offset(conn.get(), id.id, data.size());
      ngtcp2_conn_extend_max_offset(conn.get(), data.size());
    }
    return 0;
  }

  void
  Connection::stream_closed(StreamID id, uint64_t app_code)
  {
    assert(ngtcp2_is_bidi_stream(id.id));
    log::debug(logcat, "{} closed with code {}", id, app_code);
    auto it = streams.find(id);
    if (it == streams.end())
      return;
    auto& stream = *it->second;
    const bool was_closing = stream.is_closing;
    stream.is_closing = stream.is_shutdown = true;
    if (!was_closing && stream.close_callback)
    {
      log::debug(logcat, "Invoke stream close callback");
      std::optional<uint64_t> code;
      if (app_code != 0)
        code = app_code;
      stream.close_callback(stream, code);
    }

    log::debug(logcat, "Erasing stream {} from {}", id, (void*)it->second.get());
    streams.erase(it);

    if (!ngtcp2_conn_is_local_stream(conn.get(), id.id))
      ngtcp2_conn_extend_max_streams_bidi(conn.get(), 1);

    io_ready();  // Probably superfluous but sometimes we might need to send a FIN or something.
  }

  int
  Connection::stream_ack(StreamID id, size_t size)
  {
    if (auto it = streams.find(id); it != streams.end())
    {
      it->second->acknowledge(size);
    }
    return 0;
  }

  Server*
  Connection::server()
  {
    return dynamic_cast<Server*>(&endpoint);
  }

  Client*
  Connection::client()
  {
    return dynamic_cast<Client*>(&endpoint);
  }

  int
  Connection::setup_server_crypto_initial()
  {
    auto* s = server();
    assert(s);
    s->null_crypto.server_initial(conn.get());
    io_ready();
    return 0;
  }

  ConnectionID
  Connection::make_alias_id(size_t cidlen)
  {
    return endpoint.add_connection_id(*this, cidlen);
  }

  int
  Connection::get_handshake_confirmed()
  {
    if (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED)
      return 0;

    return CALLBACK_FAIL;
  }

  int
  Connection::complete_handshake()
  {
    log::debug(logcat, "Calling {}", __PRETTY_FUNCTION__);
    if (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED)
      return 0;

    endpoint.null_crypto.install_rx_key(conn.get());

    if (not ngtcp2_conn_is_server(conn.get()))
    {
      if (not endpoint.null_crypto.install_tx_key(conn.get()))
      {
        return CALLBACK_FAIL;
      }
    }

    ngtcp2_conn_handshake_completed(conn.get());

    if (on_handshake_complete)
    {
      on_handshake_complete(*this);
      on_handshake_complete = nullptr;
    }

    return 0;
  }

  int
  Connection::get_streams_available()
  {
    uint64_t left = ngtcp2_conn_get_streams_bidi_left(conn.get());
    constexpr int max_int = std::numeric_limits<int>::max();
    if (left > static_cast<uint64_t>(max_int))
      return max_int;
    return static_cast<int>(left);
  }

  const std::shared_ptr<Stream>&
  Connection::open_stream(Stream::data_callback_t data_cb, Stream::close_callback_t close_cb)
  {
    std::shared_ptr<Stream> stream{new Stream{
        *this, std::move(data_cb), std::move(close_cb), endpoint.default_stream_buffer_size}};
    if (int rv = ngtcp2_conn_open_bidi_stream(conn.get(), &stream->stream_id.id, stream.get());
        rv != 0)
      throw std::runtime_error{"Stream creation failed: "s + ngtcp2_strerror(rv)};

    auto& str = streams[stream->stream_id];
    str = std::move(stream);

    return str;
  }

  const std::shared_ptr<Stream>&
  Connection::get_stream(StreamID s) const
  {
    return streams.at(s);
  }

  int
  Connection::init_client()
  {
    endpoint.null_crypto.client_initial(conn.get());

    if (int rv = send_magic(NGTCP2_CRYPTO_LEVEL_INITIAL); rv != 0)
      return rv;
    if (int rv = send_transport_params(NGTCP2_CRYPTO_LEVEL_INITIAL); rv != 0)
      return rv;

    io_ready();
    return 0;
  }

  int
  Connection::recv_initial_crypto(std::basic_string_view<uint8_t> data)
  {
    if (data.substr(0, handshake_magic.size()) != handshake_magic)
    {
      log::warning(logcat, "Invalid initial crypto frame: did not find expected magic prefix");
      return NGTCP2_ERR_CALLBACK_FAILURE;
    }
    data.remove_prefix(handshake_magic.size());

    const bool is_server = ngtcp2_conn_is_server(conn.get());
    if (is_server)
    {
      // For a server, we receive the transport parameters in the initial packet (prepended by the
      // magic that we just removed):
      if (auto rv = recv_transport_params(data); rv != 0)
        return rv;
    }
    else
    {
      // For a client our initial crypto data should be just the magic string (the packet also
      // contains transport parameters, but they are at HANDSHAKE crypto level and so will result
      // in a second callback to handle them).
      if (!data.empty())
      {
        log::warning(logcat, "Invalid initial crypto frame: unexpected post-magic data found");
        return NGTCP2_ERR_CALLBACK_FAILURE;
      }
    }

    if (not endpoint.null_crypto.install_rx_handshake_key(conn.get()))
      log::debug(logcat, "Call to install_rx_handshake_key unsuccessful at {}", __LINE__);
    if (not endpoint.null_crypto.install_tx_handshake_key(conn.get()))
      log::debug(logcat, "Call to install_tx_handshake_key unsuccessful at {}", __LINE__);
    if (is_server)
    {
      if (not endpoint.null_crypto.install_tx_key(conn.get()))
        log::debug(logcat, "Call to install_tx_key unsuccessful at {}", __LINE__);
    }
    return 0;
  }

  // ngtcp2 doesn't expose the varint encoding, but it's fairly simple:
  // 0bXXyyyyyy -- XX indicates the encoded size (00=1, 01=2, 10=4, 11=8) and the rest of the bits
  // (6, 14, 30, or 62) are the number, with bytes in network order for >6-bit values.

  // Returns {value, consumed} where consumed is the number of bytes consumed, or 0 on failure.
  static constexpr std::pair<uint64_t, size_t>
  decode_varint(std::basic_string_view<uint8_t> data)
  {
    std::pair<uint64_t, size_t> result = {0, 0};
    auto& [val, enc_size] = result;
    if (data.empty())
      return result;
    enc_size = 1 << (data[0] >> 6);  // first two bits are log₂ of the length
    if (data.size() < enc_size)
    {
      enc_size = 0;
      return result;
    }
    val = data[0] & 0b0011'1111;
    for (size_t i = 1; i < enc_size; i++)
      val = (val << 8) | data[i];
    return result;
  }

  // Encodes an unsigned integer in QUIC encoding format; return the bytes and the length (bytes
  // beyond `length` are uninitialized).
  static constexpr std::pair<std::array<uint8_t, 8>, uint8_t>
  encode_varint(uint64_t val)
  {
    assert(val < (1ULL << 62));
    std::pair<std::array<uint8_t, 8>, uint8_t> result;
    uint8_t size = val < (1ULL << 6) ? 0 : val < (1ULL << 14) ? 1 : val < (1ULL << 30) ? 2 : 3;
    auto& [enc, len] = result;
    len = 1 << size;
    for (uint8_t i = 1; i <= len; i++)
    {
      enc[len - i] = val & 0xff;
      val >>= 8;
    }
    enc[0] = (enc[0] & 0b00'111111) | (size << 6);
    enc[0] |= size << 6;
    return result;
  }

  // We add some lokinet-specific data into the transport request and *always* as the first
  // transport parameter, but we do it in a way that the parameter gets ignored by the QUIC
  // protocol, which encodes as {varint[code], varint[length], data}, and requires a code value
  // 31*N+27 (for integer N).  Naturally we use N=42, which gives us 1329=0b10100110001 which
  // encodes in QUIC as 0b01000101 0b00110001 (the first two bits of the first byte give the integer
  // size, and the rest are the value in network order).
  static constexpr uint64_t lokinet_transport_param_N = 42;
  static constexpr auto lokinet_metadata_code_raw =
      encode_varint(31 * lokinet_transport_param_N + 27);
  static constexpr std::basic_string_view<uint8_t> lokinet_metadata_code{
      lokinet_metadata_code_raw.first.data(), lokinet_metadata_code_raw.second};
  static_assert(
      lokinet_metadata_code.size() == 2 && lokinet_metadata_code[0] == 0b01000101
      && lokinet_metadata_code[1] == 0b00110001);

  int
  Connection::recv_transport_params(std::basic_string_view<uint8_t> data)
  {
    if (data.substr(0, lokinet_metadata_code.size()) != lokinet_metadata_code)
    {
      log::warning(logcat, "transport params did not begin with expected lokinet metadata");
      return NGTCP2_ERR_TRANSPORT_PARAM;
    }
    auto [meta_len, meta_len_bytes] = decode_varint(data.substr(lokinet_metadata_code.size()));
    if (meta_len_bytes == 0)
    {
      log::warning(logcat, "transport params lokinet metadata has truncated size");
      return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
    }
    std::string_view lokinet_metadata{
        reinterpret_cast<const char*>(
            data.substr(lokinet_metadata_code.size() + meta_len_bytes).data()),
        static_cast<std::size_t>(meta_len)};
    log::debug(logcat, "Received bencoded lokinet metadata: {}", buffer_printer{lokinet_metadata});

    uint16_t port;
    try
    {
      oxenc::bt_dict_consumer meta{lokinet_metadata};
      // '#' contains the port the client wants us to forward to
      if (!meta.skip_until("#"))
      {
        log::warning(logcat, "transport params # (port) is missing but required");
        return NGTCP2_ERR_TRANSPORT_PARAM;
      }
      port = meta.consume_integer<uint16_t>();
      if (port == 0)
      {
        log::warning(logcat, "transport params tunnel port (#) is invalid: 0 is not permitted");
        return NGTCP2_ERR_TRANSPORT_PARAM;
      }
      log::debug(logcat, "decoded lokinet tunnel port = {}", port);
    }
    catch (const oxenc::bt_deserialize_invalid& c)
    {
      log::warning(logcat, "transport params lokinet metadata is invalid: {}", c.what());
      return NGTCP2_ERR_TRANSPORT_PARAM;
    }

    const bool is_server = ngtcp2_conn_is_server(conn.get());

    if (is_server)
    {
      tunnel_port = port;
    }
    else
    {
      // Make sure the server reflected the proper port
      if (tunnel_port != port)
      {
        log::warning(
            logcat, "server returned invalid port; expected {}, got {}", tunnel_port, port);
        return NGTCP2_ERR_TRANSPORT_PARAM;
      }
    }

    auto rv = ngtcp2_conn_decode_remote_transport_params(conn.get(), data.data(), data.size());
    log::debug(
        logcat,
        "Decode transport params {}",
        rv == 0 ? "success" : "fail: "s + ngtcp2_strerror(rv));

    if (rv != 0)
    {
      ngtcp2_conn_set_tls_error(conn.get(), rv);
      return rv;
    }

    const auto* params = ngtcp2_conn_get_remote_transport_params(conn.get());
    if (params == nullptr)
      log::error(
          logcat,
          "conn_get_remote_transport_params returned NULL after decode_remote_transport_params");
    else
    {
      log::trace(logcat, "params orig dcid = {}", ConnectionID(params->original_dcid));
      log::trace(logcat, "params init scid = {}", ConnectionID(params->initial_scid));
    }

    return 0;
  }

  // Sends our magic string at the given level.  This fixed magic string is taking the place of TLS
  // parameters in full QUIC.
  int
  Connection::send_magic(ngtcp2_crypto_level level)
  {
    return ngtcp2_conn_submit_crypto_data(
        conn.get(), level, handshake_magic.data(), handshake_magic.size());
  }

  template <typename String>
  static void
  copy_and_advance(uint8_t*& buf, const String& s)
  {
    static_assert(sizeof(typename String::value_type) == 1, "not a char-compatible type");
    std::memcpy(buf, s.data(), s.size());
    buf += s.size();
  }

  // Sends transport parameters.  `level` is expected to be INITIAL for clients (which send the
  // transport parameters in the initial packet), or HANDSHAKE for servers.
  int
  Connection::send_transport_params(ngtcp2_crypto_level level)
  {
    const auto* tparams = ngtcp2_conn_get_local_transport_params(conn.get());

    if (tparams == nullptr)
      throw std::runtime_error{
          "conn_get_local_transport_params returned NULL, local params not set?"};

    assert(conn_buffer.empty());
    conn_buffer.resize(Endpoint::max_pkt_size_v4);

    auto* buf = u8data(conn_buffer);
    auto* bufend = buf + conn_buffer.size();
    {
      // Send our first parameter, the lokinet metadata, in a QUIC-compatible way (by using a
      // reserved field code that QUIC parsers must ignore); currently we only include the port in
      // here (from the client to tell the server what it's trying to reach, and reflected from
      // the server for the client to verify).
      std::string lokinet_metadata = bt_serialize(oxenc::bt_dict{
          {"#", tunnel_port},
      });
      copy_and_advance(buf, lokinet_metadata_code);
      auto [bytes, size] = encode_varint(lokinet_metadata.size());
      copy_and_advance(buf, std::basic_string_view{bytes.data(), size});
      copy_and_advance(buf, lokinet_metadata);
      assert(buf < bufend);
    }

    const bool is_server = ngtcp2_conn_is_server(conn.get());
    auto exttype = is_server ? NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS
                             : NGTCP2_TRANSPORT_PARAMS_TYPE_CLIENT_HELLO;

    if (ngtcp2_ssize nwrite = ngtcp2_encode_transport_params(buf, bufend - buf, exttype, tparams);
        nwrite >= 0)
    {
      assert(nwrite > 0);
      conn_buffer.resize(buf - u8data(conn_buffer) + nwrite);
    }
    else
    {
      conn_buffer.clear();
      return nwrite;
    }
    log::debug(logcat, "encoded transport params: {}", buffer_printer{conn_buffer});
    return ngtcp2_conn_submit_crypto_data(
        conn.get(), level, u8data(conn_buffer), conn_buffer.size());
  }

}  // namespace llarp::quic
