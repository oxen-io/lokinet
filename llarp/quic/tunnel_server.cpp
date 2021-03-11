#include "tunnel_server.hpp"
#include "tunnel.hpp"
#include "connection.hpp"
#include "server.hpp"
#include <llarp/util/logging/logger.hpp>

#include <util/str.hpp>

#include <uvw/tcp.h>

using namespace std::literals;

namespace llarp::quic::tunnel
{
  IncomingTunnel::IncomingTunnel(uint16_t localhost_port)
      : IncomingTunnel{
          [localhost_port](
              [[maybe_unused]] const auto& remote, uint16_t port, SockAddr& connect_to) {
            if (port != localhost_port)
              return AcceptResult::DECLINE;
            connect_to.setIPv4(127, 0, 0, 1);
            connect_to.setPort(port);
            return AcceptResult::ACCEPT;
          }}
  {}

  int
  usage(std::string_view arg0, std::string_view msg)
  {
    std::cerr << msg << "\n\n"
              << "Usage: " << arg0
              << " [LISTENPORT [ALLOWED ...]]\n\nDefaults to listening on 4242 and allowing "
                 "22,80,4444,8080\n";
    return 1;
  }

  int
  main(int argc, char* argv[])
  {
    uint16_t listen_port = 4242;
    std::set<uint16_t> allowed_ports{{22, 80, 4444, 8080}};

    if (argc >= 2 && !parse_int(argv[1], listen_port))
      return usage(argv[0], "Invalid port "s + argv[1]);
    if (argc >= 3)
    {
      allowed_ports.clear();
      for (int i = 2; i < argc; i++)
      {
        if (argv[i] == "all"sv)
        {
          allowed_ports.clear();
          break;
        }
        uint16_t port;
        if (!parse_int(argv[i], port))
          return usage(argv[0], "Invalid port "s + argv[i]);
        allowed_ports.insert(port);
      }
    }

    auto loop = uvw::Loop::create();

    Address listen_addr{{0, 0, 0, 0}, listen_port};

    signal(SIGPIPE, SIG_IGN);

    // The local address we connect to for incoming connections.  (localhost for this demo, should
    // be the localhost.loki address for lokinet).
    std::string localhost = "127.0.0.1";

    LogInfo("Initializing QUIC server");
    llarp::quic::Server s{
        listen_addr,
        loop,
        [loop, localhost, allowed_ports](
            llarp::quic::Server&, llarp::quic::Stream& stream, uint16_t port) {
          LogDebug(
              "New incoming quic stream ",
              stream.id(),
              " to reach ",
              localhost,
              ":",
              port);
          if (port == 0 || !(allowed_ports.empty() || allowed_ports.count(port)))
          {
            LogWarn(
                "quic stream denied by configuration: ", port, " is not a permitted local port");
            return false;
          }

          stream.close_callback = [](llarp::quic::Stream& strm,
                                     std::optional<uint64_t> error_code) {
            LogDebug(
                error_code ? "Remote side" : "We",
                " closed the quic stream, closing localhost tcp connection");
            if (error_code && *error_code > 0)
              LogWarn("Remote quic stream was closed with error code ", *error_code);
            auto tcp = strm.data<uvw::TCPHandle>();
            if (!tcp)
              LogDebug("Local TCP connection already closed");
            else
              tcp->close();
          };
          // Try to open a TCP connection to the configured localhost port; if we establish a
          // connection then we immediately send a CONNECT_INIT back down the stream; if we fail
          // then we send a fail-to-connect error code.  Once we successfully connect both of
          // these handlers get replaced with the normal tunnel handlers.
          auto tcp = loop->resource<uvw::TCPHandle>();
          auto error_handler = tcp->once<uvw::ErrorEvent>(
              [&stream, localhost, port](const uvw::ErrorEvent&, uvw::TCPHandle&) {
                LogWarn(
                    "Failed to connect to ", localhost, ":", port, ", shutting down quic stream");
                stream.close(tunnel::ERROR_CONNECT);
              });
          tcp->once<uvw::ConnectEvent>(
              [streamw = stream.weak_from_this(), error_handler = std::move(error_handler)](
                  const uvw::ConnectEvent&, uvw::TCPHandle& tcp) {
                auto peer = tcp.peer();
                auto stream = streamw.lock();
                if (!stream)
                {
                  LogWarn(
                      "Connected to ",
                      peer.ip,
                      ":",
                      peer.port,
                      " but quic stream has gone away; resetting local connection");
                  tcp.closeReset();
                  return;
                }
                LogDebug(
                    "Connected to ",
                    peer.ip,
                    ":",
                    peer.port,
                    " for quic ",
                    stream->id());
                tcp.erase(error_handler);
                tunnel::install_stream_forwarding(tcp, *stream);
                assert(stream->used() == 0);

                stream->append_buffer(new std::byte[1]{tunnel::CONNECT_INIT}, 1);
                tcp.read();
              });

          // FIXME, need to configure this
          tcp->connect("127.0.0.1", port);

          return true;
        }};
    s.default_stream_buffer_size = 0;  // We steal uvw's provided buffers
    LogDebug("Initialized server");
    std::cout << "Listening on localhost:" << listen_port
              << " with tunnel(s) to localhost port(s):";
    if (allowed_ports.empty())
      std::cout << " (any)";
    for (auto p : allowed_ports)
      std::cout << ' ' << p;
    std::cout << '\n';

    loop->run();

    return 0;
  }

}  // namespace llarp::quic::tunnel
