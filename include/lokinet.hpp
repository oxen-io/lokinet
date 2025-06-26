#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <thread>
#include <type_traits>

namespace llarp
{
    struct Context;
    struct Config;
}  // namespace llarp

namespace lokinet
{
    enum class Network
    {
        MAINNET,
        TESTNET
    };

    /// Metadata returned when establishing a session for a TCP or UDP tunnel.
    struct tunnel_info
    {
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

        std::thread run_thread;

        struct path_tag
        {};
        Lokinet(path_tag, const std::filesystem::path& p);

      public:
        // Starts an embedded lokinet that loads the given string contents as a config file.
        explicit Lokinet(std::string_view config);

        // Starts an embedded lokinet instance with extra configuration specified in the given
        // config file.
        template <typename FSPath>  // Templatize to avoid ambiguous implicit conversion from std::string
            requires std::same_as<std::filesystem::path, std::remove_cvref_t<FSPath>>
        explicit Lokinet(FSPath&& config) : Lokinet{path_tag{}, config}
        {}

        // Starts an embedded lokinet with default config that runs on the given network with
        // default settings.
        explicit Lokinet(Network network);

        // Destructor stops the lokinet instance.  The destructor blocks until shutdown is complete.
        ~Lokinet();

        // Establishes a UDP session to the given remote (.loki or .snode) and port.  When the
        // lokinet session to the remote is established, the callback is invoked with the info
        // corresponding to the session and tunnel.  This call can happen instantly (before this
        // function call returns) if a session to the given address is already established, but
        // otherwise the callback will be called at some future point when the callback is
        // established.
        //
        // If a connection cannot be established for whatever reason, the `on_failed` callback is
        // invoked with a string giving a descriptive reason.  Like `on_established`, it is possible
        // for this to fire immediately, such as for an unparseable address or if Lokinet can
        // determine immediately that the connection will fail.
        //
        // The callbacks must not block as they are called from Lokinet's logic thread (and so any
        // blocking will stall Lokinet).
        void establish_udp(
            std::string remote,
            uint16_t port,
            std::function<void(tunnel_info info)> on_established,
            std::function<void(std::string_view errmsg)> on_failed);

        // Simple synchronous wrapper around the above: this blocks until either `on_established` or
        // `on_failed` is called then returns the tunnel info (success) or throws the error message
        // (failure).  This is provided for quick-and-dirty implementation code; generally code
        // should prefer the async version, above.
        tunnel_info establish_udp_blocking(std::string remote, uint16_t port);
    };
}  // namespace lokinet
