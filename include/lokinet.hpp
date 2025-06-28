#pragma once

#include <memory>
#include <thread>
#include <cstdint>
#include <functional>

namespace llarp
{
  struct Context;
  struct Config;
}  // namespace llarp

namespace lokinet
{
  /// Metadata returned when establishing a session for a TCP or UDP tunnel.
  struct tunnel_info {
    /// The requested remote address.  If an ONS entry was requested, this will be the resolved
    /// "fulladdress.loki" rather than the ONS entry value.
    std::string remote;

    /// The requested remote port.  Packets sent to the `local_port` are delivered to this
    /// remote port, and returning packets from that remote back to the incoming source are
    /// routed back to the client and delivered to the source port that sent the original
    /// packet.  Multiple connections to the same address are possible: each different source
    /// port establishes a separate connection (actual TCP connections for TCP, a remembered
    /// mapping for UDP).
    uint16_t remote_port;

    /// The bound local port.  After establishing a lokinet session, clients connect (TCP) or
    /// send (UDP) to this port (on address 127.0.0.1) to reach the destination through lokinet.
    uint16_t local_port;

    /// A suggested maximum MTU for the connection.  If the application supports a configurable
    /// MTU, this value is the recommended value that avoids some additional overhead from
    /// packet splitting, which can slightly reduce latency and jitter.  If the application
    /// doesn't support MTU configuration then this value can simply be ignored and Lokinet will
    /// split any "too large" packets into two.
    uint16_t suggested_mtu;
  };

  class Lokinet
  {
    std::shared_ptr<llarp::Context> context;
    std::shared_ptr<llarp::Config> config;

    std::thread run_thread{};

    public:

    Lokinet(const std::string& net_id = "testnet");

    ~Lokinet();

    // only applies if called before run()
    void set_log_level(const std::string& level);

    void run();

    uint16_t udp_session(const std::string& remote, uint16_t port);

    void map_tcp_remote_port(const std::string& remote, uint16_t port, std::function<void(tunnel_info)> success_cb, std::function<void(std::string)> failure_cb);
  };
}  // namespace lokinet
