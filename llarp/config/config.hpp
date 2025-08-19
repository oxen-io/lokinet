#pragma once

#include "definition.hpp"
#include "ini.hpp"
#include "llarp/constants/path.hpp"

#include <llarp/address/address.hpp>
#include <llarp/address/ip_range.hpp>
#include <llarp/auth/auth.hpp>
#include <llarp/auth/file.hpp>
#include <llarp/bootstrap.hpp>
#include <llarp/constants/files.hpp>
#include <llarp/contact/relay_contact.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/dns/srv_data.hpp>
#include <llarp/net/platform.hpp>
#include <llarp/net/policy.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/str.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace llarp
{
    using SectionValues = llarp::ConfigParser::SectionValues;
    using ConfigMap = llarp::ConfigParser::ConfigMap;

    inline constexpr uint16_t DEFAULT_CLIENT_PORT{1091};
    inline constexpr uint16_t DEFAULT_RELAY_PORT{1090};
    inline const quic::Address DEFAULT_CLIENT_ADDR{"0.0.0.0", DEFAULT_CLIENT_PORT};
    inline constexpr uint16_t DEFAULT_DNS_PORT{53};
    inline constexpr int CLIENT_ROUTER_CONNECTIONS{4};

    // TODO: don't use these maps. they're sloppy and difficult to follow
    /// Small struct to gather all parameters needed for config generation to reduce the number of
    /// parameters that need to be passed around.
    struct ConfigGenParameters
    {
        ConfigGenParameters() = default;
        virtual ~ConfigGenParameters() = default;

        ConfigGenParameters(const ConfigGenParameters&) = delete;
        ConfigGenParameters(ConfigGenParameters&&) = delete;

        config::Type type;
        fs::path default_data_dir;

        /// get network platform (virtual for unit test mocks)
        virtual const llarp::net::Platform* net_ptr();
    };

    struct RouterConfig
    {
        int client_router_connections{CLIENT_ROUTER_CONNECTIONS};

        NetID net_id = NetID::MAINNET;

        fs::path data_dir;

        bool block_bogons = false;

        int worker_threads = -1;
        int net_threads = -1;

        size_t job_que_size = 0;

        std::optional<fs::path> rc_file;

        bool is_relay = false;

        std::optional<quic::Address> public_addr;

        void define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params);
    };

    /// config for path hop selection
    struct PathConfig
    {
        /// Number of paths to maintain for inbound reachability and network queries (such as
        /// looking up client contacts).
        int inbound_paths = 4;

        /// Length of the "inbound" paths we use for inbound connections and network queries.
        /// If unset, use client_hops.
        std::optional<int> inbound_hops_;

        // Retrieves the above, with built-in fallback to the client_hops value if not set.
        int inbound_hops() const { return inbound_hops_.value_or(client_hops); }

        /// Number of paths to maintain to *each* outgoing remote (relay or snode).
        int outbound_paths = 2;

        /// Number of hops when establishing a session to a relay (i.e. to a .snode, not *through* a
        /// relay to reach a client).
        std::optional<int> relay_hops_;

        /// Retrieves the working value for relay-hops: the value if explicitly set, else one more
        /// than the configured client hops.
        int relay_hops() const { return relay_hops_.value_or(std::min(client_hops + 1, path::BUILD_LENGTH)); }

        /// Number of hops when building an aligned path to a relay to reach a client on the other
        /// side.
        int client_hops = 3;

        /// in our hops what netmask will we use for unique ips for hops
        /// i.e. 32 for every hop unique ip, 24 unique /24 per hop, etc
        uint8_t unique_hop_netmask{0};

        // TODO: some day, if we ever support routers using IPv6, there would need to be a different
        // ipv6 netmask value.

        std::chrono::seconds min_expiry = 1min;
        std::chrono::seconds acceptable_expiry = 5min;

        std::chrono::milliseconds build_timeout{10s};

        void define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params);
    };

    /** TODO:
        - finalize supervenience of ExitConfig over deprecated config entries
     */

    /// Config options related to exit node services
    struct ExitConfig
    {
        bool exit_enabled{false};

        // Used by RemoteHandler to provide auth tokens for remote exits
        std::unordered_map<NetworkAddress, std::string> auth_tokens;
        std::unordered_map<std::string, std::string> sns_auth_tokens;

        net::ExitPolicy exit_policy;

        // Remote client ONS exit addresses mapped to local IP ranges pending ONS address resolution
        // Reserved local IP ranges mapped to remote client ONS addresses (pending ONS resolution)
        std::unordered_map<std::string, std::vector<std::variant<ipv4_range, ipv6_range>>> sns_ranges;

        // Reserved local IP ranges mapped to remote client exit addresses
        std::unordered_map<NetworkAddress, std::vector<std::variant<ipv4_range, ipv6_range>>> ranges;

        void define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params);
    };

    struct NetworkConfig
    {
        bool enable_profiling{false};
        bool save_profiles{false};
        std::unordered_set<RouterID> pinned_edges;

        std::optional<fs::path> keyfile;

        bool enable_ipv6{false};
        bool is_reachable{false};

        std::set<RouterID> snode_blacklist;

        /*   Auth specific config   */
        auth::AuthType auth_type = auth::AuthType::NONE;
        auth::AuthFileType auth_file_type = auth::AuthFileType::HASHES;

        std::optional<std::string> auth_endpoint;
        std::optional<std::string> auth_method;

        std::unordered_set<NetworkAddress> auth_whitelist;

        std::unordered_set<std::string> auth_static_tokens;

        std::vector<fs::path> auth_files;

        std::unordered_set<llarp::dns::SRVData> srv_records;

        /* TESTNET: Under modification */

        // Contents of this file are read directly into ::_reserved_local_addrs
        std::optional<fs::path> addr_map_persist_file;

        // the only member that refers to an actual interface
        std::optional<std::string> _if_name;

        std::optional<ipv4_net> _local_ip_net;    // [network]:ifaddr
        std::optional<ipv6_net> _local_ipv6_net;  // [network]:ipv6

        // Remote exit or hidden service addresses mapped to fixed local IP addresses
        // TODO:
        //  - load directly into TunEndpoint mapping
        //      - when a session is created, check mapping when assigning IP's
        std::unordered_map<NetworkAddress, ipv4> _reserved_local_ipv4;
        std::unordered_map<NetworkAddress, ipv6> _reserved_local_ipv6;

        // TESTNET: moved into ExitConfig!
        bool allow_exit{false};
        // Used by RemoteHandler to provide auth tokens for remote exits
        std::unordered_map<NetworkAddress, std::string> exit_auths;
        std::unordered_map<std::string, std::string> sns_exit_auths;
        std::optional<net::ExitPolicy> traffic_policy;

        // TESTNET: move into ExitConfig!
        bool enable_route_poker{false};
        bool blackhole_routes{false};

        void define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params);
    };

    struct DnsConfig
    {
        bool l3_intercept{false};

        std::vector<fs::path> hostfiles;

        /* TESTNET: Under modification */
        std::vector<quic::Address> _upstream_dns;
        quic::Address _default_dns{"9.9.9.10", DEFAULT_DNS_PORT};
        std::optional<quic::Address> _query_bind;
        std::vector<quic::Address> _bind_addrs;

        // Deprecated
        // std::vector<SockAddr_deprecated> upstream_dns;
        // std::optional<SockAddr_deprecated> query_bind;
        // std::vector<SockAddr_deprecated> bind_addr;
        /*************************************/

        std::unordered_multimap<std::string, std::string> extra_opts;

        void define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params);
    };

    struct LinksConfig
    {
        // DEPRECATED -- use [router]:public_addr/port instead
        std::optional<quic::Address> public_addr;

        std::optional<quic::Address> listen_addr;

        void define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params);
    };

    struct ApiConfig
    {
        bool enable_rpc_server = false;
        std::vector<std::string> rpc_bind_addrs;

        void define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params);
    };

    struct LokidConfig
    {
        fs::path id_keyfile;
        std::string rpc_addr;
        bool disable_testing = false;

        void define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params);
    };

    struct BootstrapConfig
    {
        std::vector<fs::path> files;
        bool seednode{false};

        void define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params);
    };

    struct LoggingConfig
    {
        // Log type.  If nullopt then lokinet will not set up logging sinks at all (this is
        // primarily aimed at embedded clients that have already set up logging).
        std::optional<log::Type> type = log::Type::Print;

        // levels can either be just a level ("warn"), or a list of cat levels such as:
        // "*=warning, cat1=debug, cat2*=trace".  See oxen-logging for more details.  If empty then
        // logging levels will not be set at all (and, again, is most useful for embedded clients).
        std::string levels;

        std::string file;

        void define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params);
    };

    struct Config
    {
        // Creates a config for the given lokinet instance type (relay, full client, or embedded
        // client), loading configuration data from the given string, if given (all default config
        // otherwise).  The default data directory (if not explicit set in the given config string)
        // can optionally be provided.  If omitted (and not set in the string) it defaults to cwd.
        Config(config::Type type, std::string config = "", fs::path default_data_dir = fs::current_path());

        // Creates a config for the given lokinet instance type (relay, full client, or embedded
        // client), loading configuration data from an existing file.  The default data directory
        // (if not set in the config itself) will be the directory containing the given config file.
        Config(config::Type type, fs::path config_file);

        Config(Config&&) = default;
        Config(const Config&) = default;
        Config& operator=(Config&&) = default;
        Config& operator=(const Config&) = default;

        virtual ~Config() = default;

        /// create generation params (virtual for unit test mock)
        virtual std::unique_ptr<ConfigGenParameters> make_gen_params() const;

        RouterConfig router;
        ExitConfig exit;
        NetworkConfig network;
        PathConfig paths;
        DnsConfig dns;
        LinksConfig links;
        ApiConfig api;
        LokidConfig lokid;
        BootstrapConfig bootstrap;
        LoggingConfig logging;

        // Initialize config definition
        void init_config(ConfigDefinition& conf, const ConfigGenParameters& params);

        /// Insert config entries for backwards-compatibility (e.g. so that the config system will
        /// tolerate old values that are no longer accepted)
        ///
        /// @param conf is the config to modify
        void add_backcompat_opts(ConfigDefinition& conf);

        std::string generate_config_base();

        void save();

        void override(std::string section, std::string key, std::string value);

        void add_default(std::string section, std::string key, std::string value);

        bool relay() const { return type == config::Type::Relay; }
        bool embedded() const { return type == config::Type::EmbeddedClient; }
        bool client() const { return !relay(); }

      private:
        void load_config_data(std::string ini, std::optional<fs::path> fname = std::nullopt);

        void load_overrides(ConfigDefinition& conf) const;

        std::vector<std::array<std::string, 3>> additional;
        ConfigParser parser;
        fs::path data_dir{fs::current_path()};
        config::Type type;
    };

    // Ensures that a conf file exists, writing a default one if not present.  Only for full
    // clients/routers (i.e. not embedded clients).
    void ensure_config(fs::path dataDir, fs::path confFile, bool overwrite, config::Type type);

}  // namespace llarp
