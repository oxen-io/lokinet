#include "config.hpp"

#include "definition.hpp"
#include "ini.hpp"

#include <llarp/constants/path.hpp>
#include <llarp/constants/platform.hpp>
#include <llarp/constants/version.hpp>
#include <llarp/contact/sns.hpp>
#include <llarp/path/path_handler.hpp>
#include <llarp/util/file.hpp>
#include <llarp/util/formattable.hpp>
#include <llarp/util/logging/buffer.hpp>

#include <filesystem>
#include <stdexcept>

#ifndef LOKINET_EMBEDDED_ONLY
#include <oxenmq/address.h>
#endif

namespace llarp
{
    static auto logcat = log::Cat("config");

    static bool check_path_op(std::optional<std::filesystem::path>& path)
    {
        if (not path.has_value())
        {
            log::info(logcat, "Path input failed to parse...");
        }
        else if (path->empty())
        {
            log::warning(logcat, "Path contents ({}) empty...", path->c_str());
            path.reset();
        }
        else
        {
            log::debug(logcat, "Valid path parsed ({})", path->c_str());
            return true;
        }

        return false;
    }

    using namespace config;

    const llarp::net::Platform* ConfigGenParameters::net_ptr()
    {
#ifndef LOKINET_EMBEDDED_ONLY
        if (type != config::Type::EmbeddedClient)
            return llarp::net::Platform::Default_ptr();
#endif
        return nullptr;
    }

    static auto public_ip_loader(
        std::optional<quic::Address>& into, std::string conf_name, std::string deprecated_for = ""s)
    {
        return [&into, conf_name = std::move(conf_name), deprecated_for = std::move(deprecated_for)](std::string ip) {
            if (!deprecated_for.empty())
                log::warning(logcat, "{} is deprecated; use {} instead", conf_name, deprecated_for);
            try
            {
                quic::Address a{ip, into ? into->port() : uint16_t{0}};

                if (!a.is_ipv4())
                    throw std::invalid_argument{"IP must be an IPv4 address"};
                if (!a.is_public_ip())
                    throw std::invalid_argument{"IP is not public"};

                into = std::move(a);
            }
            catch (const std::exception& e)
            {
                throw std::invalid_argument{"Invalid {}: {}"_format(conf_name, e.what())};
            }
        };
    }
    static auto public_port_loader(
        std::optional<quic::Address>& into, std::string conf_name, std::string deprecated_for = ""s)
    {
        return [&into, conf_name = std::move(conf_name), deprecated_for = std::move(deprecated_for)](uint16_t port) {
            if (!deprecated_for.empty())
                log::warning(logcat, "{} is deprecated; use {} instead", conf_name, deprecated_for);
            if (port == 0)
                throw std::invalid_argument{"{} cannot be 0"_format(conf_name)};
            if (!into)
                into.emplace();
            into->set_port(port);
        };
    }

    void RouterConfig::define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params)
    {
        conf.add_section_comments(
            "router",
            {
                "Configuration for routing activity.",
            });

        conf.define_option<int>("router", "job-queue-size", Default{1024 * 8}, Hidden, [this](int arg) {
            if (arg < 1024)
                throw std::invalid_argument("job-queue-size must be 1024 or greater");

            job_que_size = arg;
        });

        conf.define_option<std::string>(
            "router",
            "netid",
            Default{"{}"_format(NetID::MAINNET)},
            Comment{"Network ID; this is '{}' for mainnet, '{}' for testnet."_format(NetID::MAINNET, NetID::TESTNET)},
            [this](std::string arg) { net_id = netid_from_string(arg); });

        conf.define_option<int>("router", "relay-connections", Deprecated);

        conf.define_option<int>("router", "min-connections", Deprecated);

        conf.define_option<int>("router", "max-connections", Deprecated);

        conf.define_option<std::string>("router", "nickname", Deprecated);

        conf.define_option<std::filesystem::path>(
            "router",
            "data-dir",
            Default{params.default_data_dir},
            Comment{
                "Optional directory for containing lokinet runtime data. This includes generated",
                "private keys.",
            },
            [this](std::filesystem::path arg) {
                if (arg.empty())
                    arg = std::filesystem::path{"."};
                if (not exists(arg))
                    throw std::runtime_error{"Specified [router]:data-dir {} does not exist"_format(arg)};

                data_dir = std::move(arg);
            });

        conf.define_option<std::string>(
            "router",
            "public-ip",
            RelayOnly,
            Comment{
                "For complex network configurations where the detected IP is incorrect or non-public",
                "this setting specifies the public IPv4 address at which this router reachable.",
            },
            public_ip_loader(public_addr, "[router]:public-address"));

        conf.define_option<std::string>("router", "public-address", Hidden, [](std::string) {
            throw std::invalid_argument{
                "[router]:public-address option no longer supported, use [router]:public-ip and "
                "[router]:public-port instead"};
        });

        conf.define_option<uint16_t>(
            "router",
            "public-port",
            RelayOnly,
            Comment{
                "When specifying public-ip=, this specifies the public UDP port at which this lokinet",
                "router is reachable. Defaults to the [bind]:listen port when public-ip is specified.",
            },
            public_port_loader(public_addr, "[router]:public-port"));

        conf.add_options_validator([this] {
            if (public_addr and not public_addr->is_public_ip())
                throw std::invalid_argument{"[router]:public-ip is required when specifying [router]:public-port"};
        });

        // FIXME: this option isn't currently used!
        conf.define_option<int>(
            "router",
            "worker-threads",
            Default{0},
            Comment{
                "The number of threads available for performing cryptographic functions.",
                "The minimum is one thread, but network performance may increase with more.",
                "threads. Should not exceed the number of logical CPU cores.",
                "0 means use the number of logical CPU cores detected at startup.",
            },
            [this](int arg) {
                if (arg < 0)
                    throw std::invalid_argument("worker-threads must be >= 0");

                worker_threads = arg;
            });

        // Hidden option because this isn't something that should ever be turned off occasionally
        // when doing dev/testing work.
        conf.define_option<bool>("router", "block-bogons", Default{true}, Hidden, assignment_acceptor(block_bogons));

        conf.define_option<std::string>("router", "contact-file", Deprecated);

        conf.define_option<std::string>("router", "encryption-privkey", Deprecated);

        conf.define_option<std::string>("router", "ident-privkey", Deprecated);

        conf.define_option<std::string>("router", "transport-privkey", RelayOnly, Deprecated);

        // Deprecated options:

        // these weren't even ever used!
        conf.define_option<std::string>("router", "max-routers", Deprecated);
        conf.define_option<std::string>("router", "min-routers", Deprecated);

        // TODO: this may have been a synonym for [router]worker-threads
        conf.define_option<std::string>("router", "threads", Deprecated);
        conf.define_option<std::string>("router", "net-threads", Deprecated);

        is_relay = params.type == config::Type::Relay;
    }

    void ExitConfig::define_config_options(ConfigDefinition& conf, const ConfigGenParameters&)
    {
        conf.define_option<std::string>(
            "exit",
            "auth",
            FullClientOnly,
            MultiValue,
            Comment{
                "Specify an optional authentication token required to use a non-public exit node.",
                "For example:",
                "    auth=myfavouriteexit.loki:abc",
                "uses the authentication code `abc` whenever myfavouriteexit.loki is accessed.",
                "Can be specified multiple times to store codes for different exit nodes.",
            },
            [this](std::string arg) {
                if (arg.empty())
                    throw std::invalid_argument{"Empty argument passed to '[exit]:auth'"};

                const auto pos = arg.find(":");

                if (pos == std::string::npos)
                {
                    throw std::invalid_argument(
                        "[exit]:auth invalid format, expects exit-address.loki:auth-token-goes-here");
                }

                const auto addr = arg.substr(0, pos);
                auto auth = arg.substr(pos + 1);

                if (is_valid_sns(addr))
                {
                    sns_auth_tokens.emplace(std::move(addr), std::move(auth));
                    return;
                }
                try
                {
                    NetworkAddress exit{addr};
                    if (!exit.client())
                        throw std::invalid_argument{"only .loki addresses can be used for exits"};
                    auth_tokens.emplace(std::move(exit), std::move(auth));
                }
                catch (const std::exception& e)
                {
                    throw std::invalid_argument("[exit]:auth invalid exit address: {}"_format(e.what()));
                }
            });

        conf.define_option<bool>(
            "exit",
            "enable",
            FullClientOnly,
            Default{false},
            assignment_acceptor(exit_enabled),
            Comment{
                "Enable exit-node functionality for local lokinet instance.",
            });

        conf.define_option<std::string>(
            "exit",
            "policy",
            FullClientOnly,
            MultiValue,
            Comment{
                "Specifies the IP traffic accepted by the local exit node traffic policy. If any are",
                "specified then only matched traffic will be allowed and all other traffic will be",
                "dropped. Examples:",
                "    policy=tcp",
                "would allow all TCP/IP packets (regardless of port);",
                "    policy=0x69",
                "would allow IP traffic with IP protocol 0x69;",
                "    policy=udp/53",
                "would allow UDP port 53; and",
                "    policy=tcp/smtp",
                "would allow TCP traffic on the standard smtp port (21).",
            },
            [this](std::string arg) {
                // this will throw on error
                exit_policy.protocols.insert(net::ProtocolInfo::from_config(arg));
            });

        conf.define_option<std::string>(
            "exit",
            "reserved-range",
            FullClientOnly,
            MultiValue,
            Comment{
                "Reserve an ip range to use as an exit broker for a `.loki` address",
                "Specify a `.loki` address and a reserved ip range to use as an exit broker.",
                "Examples:",
                "    reserved-range=whatever.loki",
                "would route all exit traffic through whatever.loki; and",
                "    reserved-range=stuff.loki:100.0.0.0/24",
                "would route the IP range 100.0.0.0/24 through stuff.loki.",
                "This option can be specified multiple times (to map different IP ranges).",
            },
            [this](std::string arg) {
                if (arg.empty())
                    return;

                std::variant<ipv4_range, ipv6_range> range;

                const auto pos = arg.find(":");

                if (pos == std::string::npos)
                    range = ipv4{0} / 0;
                else
                {
                    try
                    {
                        std::string input = arg.substr(pos + 1);
                        if (input.find(":") != std::string::npos)  // ipv6
                            range = parse_ipv6_range(input, 128);
                        else
                            range = parse_ipv4_range(input, 32);
                    }
                    catch (const std::exception& e)
                    {
                        throw std::invalid_argument{"[exit]:reserved-range invalid ip range: {}"_format(e.what())};
                    }

                    arg.resize(pos);
                }

                if (is_valid_sns(arg))
                    sns_ranges[arg].push_back(std::move(range));
                else
                {
                    try
                    {
                        ranges[NetworkAddress{arg}].push_back(std::move(range));
                    }
                    catch (const std::exception& e)
                    {
                        throw std::invalid_argument{"[exit]:reserved-range invalid address: {}"_format(arg)};
                    }
                }
            });

        conf.define_option<std::string>(
            "exit",
            "routed-range",
            FullClientOnly,
            MultiValue,
            Comment{
                "Advertise that exit node routes exit traffic to the specified IP range. If omitted, the",
                "default is ALL public ranges.  Can be set to public to indicate that this exit",
                "routes traffic to the public internet.",
                "For example:",
                "    routed-range=10.0.0.0/16",
                "    routed-range=public",
                "to advertise that this exit routes traffic to both the public internet, and to",
                "10.0.x.y addresses.",
                "",
                "Note that this option does not automatically configure network routing; that",
                "must be configured separately on the exit system to handle lokinet traffic.",
            },
            [this](std::string arg) {
                if (arg == "public")
                    exit_policy.ranges.push_back(ipv4{0} / 0);
                else
                {
                    try
                    {
                        if (arg.find(':') != std::string::npos)
                            exit_policy.ranges_v6.push_back(parse_ipv6_range(arg, 128));
                        else
                            exit_policy.ranges.push_back(parse_ipv4_range(arg, 32));
                    }
                    catch (const std::exception& e)
                    {
                        throw std::invalid_argument{"[exit]:routed-range invalid range '{}': {}"_format(arg, e.what())};
                    }
                }
            });
    }

    void NetworkConfig::define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params)
    {
        conf.add_section_comments(
            "network",
            {
                "Network settings related to network devices and communications.",
            });

        conf.define_option<bool>(
            "network",
            "save-profiles",
            Default{params.type != config::Type::EmbeddedClient},
            Hidden,
            assignment_acceptor(save_profiles));

        conf.define_option<bool>("network", "profiling", Default{true}, Hidden, assignment_acceptor(enable_profiling));

        conf.define_option<std::string>("network", "profiles", Deprecated);

        conf.define_option<std::string>(
            "network",
            "keyfile",
            ClientOnly,
            [this](std::string arg) {
                if (arg.empty())
                    return;

                keyfile = arg;

                if (check_path_op(keyfile))
                    log::info(logcat, "Client configured to try private key file at path: {}", keyfile->c_str());
                else
                    log::warning(logcat, "Bad input for client private key file ({}); using ephemeral...", arg);
            },
            Comment{
                "The private key to persist address with. If not specified the address will be",
                "ephemerally generated.",
            });

        conf.define_option<std::string>(
            "network",
            "auth-type",
            FullClientOnly,
            Comment{
                "Set the endpoint authentication type.",
                "none/whitelist/lmq/file",
            },
            [this](std::string arg) {
                if (arg == "file")
                    auth_type = auth::AuthType::FILE;
                else if (arg == "lmq" || arg == "omq" || arg == "zmq")
                    auth_type = auth::AuthType::OMQ;
                else if (arg == "whitelist")
                    auth_type = auth::AuthType::WHITELIST;
                else if (arg == "" || arg == "none")
                    auth_type = auth::AuthType::NONE;
                else
                    throw std::invalid_argument{"invalid [network]:auth-type value: '{}'"_format(arg)};
            });

        conf.define_option<std::string>(
            "network",
            "omq-auth-endpoint",
            FullClientOnly,
            assignment_acceptor(auth_endpoint),
            Comment{
                "OMQ endpoint to talk to for authenticating new sessions",
                "ipc:///var/lib/lokinet/auth.socket",
                "tcp://127.0.0.1:5555",
            });

        conf.define_option<std::string>(
            "network",
            "omq-auth-method",
            FullClientOnly,
            Default{"llarp.auth"},
            Comment{
                "OMQ function to call for authenticating new sessions",
                "llarp.auth",
            },
            [this](std::string arg) {
                if (arg.empty())
                    return;
                auth_method = std::move(arg);
            });

        conf.define_option<std::string>(
            "network",
            "auth-whitelist",
            FullClientOnly,
            MultiValue,
            Comment{
                "manually add a remote endpoint by .loki address to the access whitelist",
            },
            [this](std::string arg) {
                try
                {
                    auth_whitelist.insert(NetworkAddress{arg});
                }
                catch (const std::exception& e)
                {
                    throw std::invalid_argument{
                        "[network]:auth-whitelist: invalid .loki address '{}': {}"_format(arg, e.what())};
                }
            });

        conf.define_option<std::filesystem::path>(
            "network",
            "auth-file",
            FullClientOnly,
            MultiValue,
            Comment{
                "Read auth tokens from file to accept endpoint auth",
                "Can be provided multiple times",
            },
            [this, rel_base = params.default_data_dir](std::filesystem::path arg) {
                if (!arg.empty() && arg.is_relative())
                    arg = rel_base / arg;
                if (not exists(arg))
                    throw std::invalid_argument{"cannot load auth file {}: file does not exist"_format(arg)};
                auth_files.push_back(std::move(arg));
            });

        conf.define_option<std::string>(
            "network",
            "auth-file-type",
            FullClientOnly,
            Comment{
                "How to interpret the contents of an auth file.",
#ifdef LOKINET_HAVE_CRYPT
                "Possible values: hash, plaintext",
#else
                "Possible values: plaintext",
#endif
            },
            [this](std::string arg) {
                if (arg == "plain" || arg == "plaintext")
                    auth_file_type = auth::AuthFileType::PLAIN;
                else if (arg == "hashed" || arg == "hashes" || arg == "hash")
                {
#ifndef LOKINET_HAVE_CRYPT
                    throw std::invalid_argument{"Hashed auth files are not supported by this Lokinet build"};
#endif
                    auth_file_type = auth::AuthFileType::HASHES;
                }
                else
                    throw std::invalid_argument{"Invalid auth file type '{}'"_format(arg)};
            });

        conf.define_option<std::string>(
            "network",
            "auth-static",
            FullClientOnly,
            MultiValue,
            Comment{
                "Manually add a static auth code to accept for endpoint auth",
                "Can be provided multiple times",
            },
            [this](std::string arg) { auth_static_tokens.emplace(std::move(arg)); });

        conf.define_option<bool>(
            "network",
            "reachable",
            FullClientOnly,
            Default{true},
            assignment_acceptor(is_reachable),
            Comment{
                "Determines whether we will pubish our service's ClientContact to the network (client default: TRUE)",
            });

        conf.define_option<int>("network", "hops", ClientOnly, Hidden, [](int) {
            log::warning(
                logcat,
                "[network]:hops is no longer supported; default path lengths applied. See the path options in the "
                "[paths] section instead");
        });

        conf.define_option<int>("network", "paths", ClientOnly, Hidden, [](int) {
            log::error(
                logcat,
                "[network]:paths is no longer supported; default path numbers applied. See the path options in the "
                "[paths] section instead");
        });

        conf.define_option<bool>(
            "network",
            "auto-routing",
            FullClientOnly,
            Default{true},
            Comment{
                "Enable / disable automatic route configuration.",
                "When this is enabled and an exit is used Lokinet will automatically configure the",
                "operating system routes to route public internet traffic through the exit node.",
                "This is enabled by default, but can be disabled if advanced/manual exit routing",
                "configuration is desired."},
            assignment_acceptor(enable_route_poker));

        conf.define_option<bool>(
            "network",
            "blackhole-routes",
            FullClientOnly,
            Default{true},
            Comment{
                "Enable / disable route configuration blackholes.",
                "When enabled lokinet will drop IPv4 and IPv6 traffic (when in exit mode) that is "
                "not",
                "handled in the exit configuration.  Enabled by default."},
            assignment_acceptor(blackhole_routes));

        conf.define_option<std::string>(
            "network",
            "ifname",
            NotEmbedded,
            Comment{
                "Interface name for lokinet traffic. If unset lokinet will look for a free name",
                "matching 'lokitunN', starting at N=0 (e.g. lokitun0, lokitun1, ...).",
#ifdef __linux__
                "",
                "On Linux, you can use '%d' in the name as a pattern to have the OS automatically choose",
                "a device name by replacing '%d' with a number to construct an unused interface name",
#endif
            },
            assignment_acceptor(_if_name));

        conf.define_option<std::string>(
            "network",
            "ifaddr",
            NotEmbedded,
            Comment{
                "Local IP and netmask for lokinet traffic. For example, 172.16.0.1/16 to use",
                "172.16.0.1 for this lokinet instance and 172.16.x.y for remote peers. If omitted",
                "then lokinet will attempt to automatically select an unused private range.",
                "If you specify an all-0 address with range (e.g. 0.0.0.0/12) then lokinet will",
                "auto-select a private range of the given size.",
            },
            [this](std::string arg) {
                try
                {
                    _local_ip_net = parse_ipv4_net(arg);
                }
                catch (const std::exception& e)
                {
                    throw std::invalid_argument{"[network]:ifaddr invalid value '{}': {}"_format(arg, e.what())};
                }
            });

        conf.define_option<std::string>(
            "network",
            "ipv6-network",
            NotEmbedded,
            Hidden,
            Comment{
                "Enables internal IPv6 traffic for lokinet.  Can be set to:",
                "  - false to disable IPv6 support.  This is the default if omitted",
                "  - true to enable IPv6 support and auto-detect a free private /64 network range",
                "  - ::/80 to auto-detect a free private range of netmask 80 (change as needed) ",
                "    instead of the default 64",
                "  - An explicit private address and range to use, such as: fd00:abcd:1234::1/56",
                "",
                "Currently experimental and not supported.",
            },
            [this](std::string arg) {
                if (arg.empty())
                {
                    enable_ipv6 = false;
                    return;
                }
                if (auto b = parse_boolean(arg))
                {
                    enable_ipv6 = *b;
                    return;
                }
                try
                {
                    _local_ipv6_net = parse_ipv6_net(arg);
                    enable_ipv6 = true;
                }
                catch (const std::exception& e)
                {
                    throw std::invalid_argument{"[network]:ipv6-addr invalid value '{}': {}"_format(arg, e.what())};
                }
            });

        conf.define_option<std::string>(
            "network",
            "mapaddr",
            FullClientOnly,
            MultiValue,
            Comment{
                "Map a remote `.loki` address to always use a fixed local IP. For example:",
                "    mapaddr=<pubkey>.loki:172.16.0.10",
                "maps `<pubkey>.loki` to `172.16.0.10` instead of using the next available IP.",
                "The given IP address must be inside the range configured by ifaddr=, and the",
                "remote `.loki` cannot be an ONS address"},
            [this](std::string arg) {
                if (arg.empty())
                    return;

                const auto pos = arg.find(":");

                if (pos == std::string::npos)
                    throw std::invalid_argument{
                        "[endpoint]:mapaddr invalid entry '{}'; expected 'ADDR:IP'"_format(arg)};

                auto addr_arg = std::string_view{arg}.substr(0, pos);
                auto ip_arg = arg.substr(pos + 1);

                try
                {
                    NetworkAddress raddr{addr_arg};
                    // ipv6
                    if (ip_arg.find(':') != std::string_view::npos)
                        _reserved_local_ipv6.emplace(raddr, ip_arg);
                    else
                        _reserved_local_ipv4.emplace(raddr, ip_arg);
                }
                catch (const std::exception& e)
                {
                    throw std::invalid_argument{"[endpoint]:mapaddr invalid entry '{}': {}"_format(arg, e.what())};
                }
            });

        // TODO: support SRV records for routers, but for now client only
        conf.define_option<std::string>(
            "network",
            "srv",
            FullClientOnly,
            MultiValue,
            Comment{
                "Specify SRV Records for services hosted on the SNApp for protocols that use SRV",
                "records for service discovery. Each line specifies a single SRV record as:",
                "    srv=_service._protocol priority weight port target.loki",
                "and can be specified multiple times as needed.",
                "For more info see",
                "https://docs.oxen.io/products-built-on-oxen/lokinet/snapps/hosting-snapps",
                "and general description of DNS SRV record configuration.",
            },
            [this](std::string arg) {
                auto maybe_srv = dns::SRVData::from_srv_string(arg);

                if (not maybe_srv)
                    throw std::invalid_argument{"Invalid SRV Record string: {}"_format(arg)};

                srv_records.emplace(std::move(*maybe_srv));
            });

        conf.define_option<int>("network", "path-alignment-timeout", Deprecated);

        conf.define_option<std::filesystem::path>(
            "network",
            "persist-addrmap-file",
            FullClientOnly,
            Comment{
                "If given this specifies a file in which to record mapped local tunnel addresses so",
                "the same local address will be used for the same lokinet address on reboot. If this",
                "is not specified then the local IP of remote lokinet targets will not persist across",
                "restarts of lokinet.",
            },
            [this, rel_base = params.default_data_dir](std::filesystem::path file) {
                if (!file.empty() && file.is_relative())
                    file = rel_base / file;
                static constexpr auto addrmap_errorstr = "Invalid entry in persist-addrmap-file"sv;
                if (file.empty())
                    throw std::invalid_argument("persist-addrmap-file cannot be empty");

                if (not exists(file))
                    throw std::invalid_argument("persist-addrmap-file path invalid: {}"_format(file));

                bool load_file = true;
                {
                    constexpr auto ADDR_PERSIST_MODIFY_WINDOW = 1min;
                    const auto last_write_time = std::filesystem::last_write_time(file);
                    const auto now = decltype(last_write_time)::clock::now();

                    if (now < last_write_time or now - last_write_time > ADDR_PERSIST_MODIFY_WINDOW)
                    {
                        load_file = false;
                    }
                }

                std::string data;
                if (auto maybe = util::OpenFileStream<std::ifstream>(file, std::ios_base::binary); maybe and load_file)
                {
                    log::debug(logcat, "Config loading persisting address map file from path:{}", file);
                    maybe->seekg(0, std::ios_base::end);
                    const auto len = maybe->tellg();
                    maybe->seekg(0, std::ios_base::beg);
                    data.resize(len);
                    maybe->read(data.data(), len);
                }
                else
                {
                    auto err = "Config could not load persisting address map file from path:{}"_format(file);
                    log::warning(logcat, "{} {}", err, load_file ? "NOT FOUND" : "STALE");
                }

                if (not data.empty())
                {
                    log::trace(logcat, "Config parsing address map data: {}", llarp::buffer_printer{data});

                    const auto parsed = oxenc::bt_deserialize<oxenc::bt_dict>(data);

                    for (const auto& [key, value] : parsed)
                    {
                        try
                        {
                            quic::Address addr{key, 0};

                            std::variant<ipv4, ipv6> ip;

                            auto check_ip_okay = []<typename Range>(const std::optional<Range>& range, const auto& ip) {
                                if (range)
                                {
                                    bool bad = ip == range->ip || ip == range->to_range().ip;
                                    if constexpr (std::same_as<Range, ipv4_net>)
                                        bad = bad || ip == range->broadcast();
                                    if (bad)
                                    {
                                        log::warning(
                                            logcat, "{}: ignore invalid address map IP {}", addrmap_errorstr, ip);
                                        return false;
                                    }
                                    if (!range->contains(ip))
                                    {
                                        log::warning(
                                            logcat,
                                            "{}: IP {} is outside the configured local range {}",
                                            addrmap_errorstr,
                                            ip,
                                            range->to_range());
                                        return false;
                                    }
                                }
                                return true;
                            };

                            if (addr.is_ipv4())
                            {
                                if (!check_ip_okay(_local_ip_net, ip.emplace<ipv4>(addr.to_ipv4())))
                                    continue;
                            }
                            else
                            {
                                if (!check_ip_okay(_local_ipv6_net, ip.emplace<ipv6>(addr.to_ipv6())))
                                    continue;
                            }

                            const auto* arg = std::get_if<std::string>(&value);
                            if (not arg)
                            {
                                log::warning(logcat, "{}: {}", addrmap_errorstr, "not a string!");
                                continue;
                            }

                            if (is_valid_sns(*arg))
                            {
                                log::warning(logcat, "{}: {}", addrmap_errorstr, "cannot accept ONS names!");
                                continue;
                            }

                            try
                            {
                                NetworkAddress netaddr{*arg};
                                if (auto* ip4 = std::get_if<ipv4>(&ip))
                                    _reserved_local_ipv4.emplace(std::move(netaddr), std::move(*ip4));
                                else
                                    _reserved_local_ipv6.emplace(std::move(netaddr), std::move(std::get<ipv6>(ip)));
                            }
                            catch (const std::exception& e)
                            {
                                log::warning(logcat, "{}: invalid value {}: {}", addrmap_errorstr, *arg, e.what());
                                continue;
                            }
                        }
                        catch (const std::exception& e)
                        {
                            log::warning(
                                logcat,
                                "Exception caught parsing key:value (key:{}) pair in addr persist file:{}",
                                key,
                                e.what());
                        }
                    }
                }

                addr_map_persist_file = file;
            });

        // Deprecated options:
        conf.define_option<std::string>("network", "enabled", Deprecated);
    }

    void DnsConfig::define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params)
    {
        conf.add_section_comments(
            "dns",
            {
                "DNS configuration",
            });

        // Most non-linux platforms have loopback as 127.0.0.1/32, but linux uses 127.0.0.1/8 so
        // that we can bind to other 127.* IPs to avoid conflicting with something else that may be
        // listening on 127.0.0.1:53.
        constexpr std::array DefaultDNSBind{
#ifdef __linux__
#ifdef WITH_SYSTEMD
            // when we have systemd support add a random high port on loopback as well
            // see https://github.com/oxen-io/lokinet/issues/1887#issuecomment-1091897282
            Default{"127.0.0.1:0"},
#endif
            Default{"127.3.2.1:53"},
#else
            Default{"127.0.0.1:53"},
#endif
        };

        auto parse_addr_for_dns = [](const std::string& arg) {
            std::optional<quic::Address> addr = std::nullopt;
            std::string_view arg_v{arg}, port;
            std::string host;
            uint16_t p{DEFAULT_DNS_PORT};

            if (auto pos = arg_v.find(':'); pos != arg_v.npos)
            {
                host = arg_v.substr(0, pos);
                port = arg_v.substr(pos + 1);

                if (not llarp::parse_int<uint16_t>(port, p))
                    log::info(logcat, "Failed to parse port in arg:{}, defaulting to DNS port 53", port);

                addr = quic::Address{host, p};
            }

            return addr;
        };

        conf.define_option<std::string>(
            "dns",
            "upstream",
            FullClientOnly,
            MultiValue,
            Comment{
                "Upstream resolver(s) to use as fallback for non-loki addresses.",
                "Multiple values accepted.",
            },
            [this, parse_addr_for_dns](std::string arg) {
                if (not arg.empty())
                {
                    if (auto maybe_addr = parse_addr_for_dns(arg))
                        _upstream_dns.push_back(std::move(*maybe_addr));
                    else
                        log::warning(logcat, "Failed to parse upstream DNS resolver address:{}", arg);
                }
            });

        conf.define_option<bool>(
            "dns",
            "l3-intercept",
            FullClientOnly,
            Default{
                platform::is_windows or platform::is_android or (platform::is_macos and not platform::is_apple_sysex)},
            Comment{"Intercept all dns traffic (udp/53) going into our lokinet network interface "
                    "instead of binding a local udp socket"},
            assignment_acceptor(l3_intercept));

        conf.define_option<std::string>(
            "dns",
            "query-bind",
            FullClientOnly,
#if defined(_WIN32)
            Default{"0.0.0.0:0"},
#else
            Hidden,
#endif
            Comment{
                "Address to bind to for sending upstream DNS requests.",
            },
            [this, parse_addr_for_dns](std::string arg) {
                if (not arg.empty())
                {
                    if (auto maybe_addr = parse_addr_for_dns(arg))
                        _query_bind = std::move(*maybe_addr);
                    else
                        log::warning(logcat, "Failed to parse bind address for DNS queries:{}", arg);
                }
            });

        conf.define_option<std::string>(
            "dns",
            "bind",
            NotEmbedded,
            DefaultDNSBind,
            MultiValue,
            Comment{
                "Address to bind to for handling DNS requests.",
            },
            [this, parse_addr_for_dns](std::string arg) {
                if (not arg.empty())
                {
                    if (auto maybe_addr = parse_addr_for_dns(arg))
                    {
                        _bind_addrs.push_back(std::move(*maybe_addr));
                    }
                    else
                        log::warning(logcat, "Failed to parse bind address for handling DNS requests:{}", arg);
                }
            });

        conf.define_option<std::filesystem::path>(
            "dns",
            "add-hosts",
            FullClientOnly,
            Comment{"Add a hosts file to the dns resolver", "For use with client side dns filtering"},
            [this, rel_base = params.default_data_dir](std::filesystem::path path) {
                if (path.empty())
                    return;
                if (path.is_relative())
                    path = rel_base / path;
                if (not exists(path))
                    throw std::invalid_argument{"cannot add hosts file {} as it does not exist"_format(path)};
                hostfiles.emplace_back(std::move(path));
            });

        // Ignored option (used by the systemd service file to disable resolvconf configuration).
        conf.define_option<bool>(
            "dns",
            "no-resolvconf",
            FullClientOnly,
            Comment{
                "Can be uncommented and set to 1 to disable resolvconf configuration of lokinet "
                "DNS.",
                "(This is not used directly by lokinet itself, but by the lokinet init scripts",
                "on systems which use resolveconf)",
            });

        // forward the rest to libunbound
        conf.add_undeclared_handler(
            "dns", [this](auto, std::string_view key, std::string_view val) { extra_opts.emplace(key, val); });
    }

    void LinksConfig::define_config_options(ConfigDefinition& conf, const ConfigGenParameters&)
    {
        conf.add_section_comments(
            "bind",
            {
                "This section allows specifying the IPs that lokinet uses for incoming and outgoing",
                "connections.  For simple setups it can usually be left blank, but may be required",
                "for routers with multiple IPs, or routers that must listen on a private IP with",
                "forwarded public traffic.  It can also be useful for clients that want to use a",
                "consistent outgoing port for which firewall rules can be configured.",
            });

        conf.define_option<std::string>(
            "bind",
            "public-ip",
            Hidden,
            RelayOnly,
            public_ip_loader(public_addr, "[bind]:public-ip", "[router]:public-ip"));

        conf.define_option<uint16_t>(
            "bind",
            "public-port",
            Hidden,
            RelayOnly,
            public_port_loader(public_addr, "[bind]:public-port", "[router]:public-port"));

        auto parse_addr_for_link = [](std::string_view arg) {
            quic::Address a = quic::Address::parse(arg, 0);
            if (a.is_loopback())
                throw std::invalid_argument{"Invalid listen address: {} is a loopback address"_format(arg)};
            if (a.is_ipv6() && a.is_any_addr())
                a = quic::Address{ipv4{0, 0, 0, 0}, a.port()};
            else if (a.is_ipv6() && a.is_ipv4_mapped_ipv6())
                a.unmap_ipv4_from_ipv6();
            else if (a.is_ipv6())
                throw std::invalid_argument{"Invalid listen address: IPv6 addresses are not currently supported"};
            return a;
        };

        conf.define_option<std::string>(
            "bind",
            "listen",
            Comment{
                "IP and/or port for lokinet to bind to for inbound/outbound connections.",
                "",
                "If IP is omitted then lokinet will search for a local network interface with a",
                "public IP address and use that IP (and will exit with an error if no such IP is found",
                "on the system).  If port is omitted then lokinet defaults to 1090 (routers) or 1091 (clients).",
                "",
                "Examples:",
                "    listen=15.5.29.5:443",
                "    listen=10.0.2.2",
                "    listen=:1234",
                "",
                "Note that, when running as a relay, a private range IP address (like the second example",
                "above) requires also using [router]:public-ip/-port to specify the public IP address at",
                "which this router can be reached, and requires that traffic on that port is redirected to",
                "the listening internal address.",
            },
            [this, parse_addr_for_link](const std::string& arg) {
                if (listen_addr)
                    throw std::runtime_error{
                        "Multiple listen addresses found.  If upgrading from an older lokinet, delete extra "
                        "[bind]:inbound and [bind]:IP and use only one [bind]:listen"};
                listen_addr = parse_addr_for_link(arg);
            });

        conf.define_option<std::string>(
            "bind", "inbound", RelayOnly, MultiValue, Hidden, [this, parse_addr_for_link](const std::string& arg) {
                if (listen_addr)
                    throw std::runtime_error{
                        "Multiple listen addresses found.  If upgrading from an older lokinet, delete extra "
                        "[bind]:inbound and [bind]:IP and use only one [bind]:listen"};
                listen_addr = parse_addr_for_link(arg);
                log::warning(
                    logcat,
                    "Loaded listen address {} from deprecated [bind]:inbound option; please update your config to "
                    "use [bind]:listen instead",
                    *listen_addr);
            });

        conf.define_option<std::string>("bind", "outbound", MultiValue, Deprecated, Hidden);

        conf.add_undeclared_handler("bind", [this](std::string_view, std::string_view key, std::string_view val) {
            // special case: old lokinet used '*' for outbound port, which now does nothing
            if (key == "*")
            {
                log::warning(
                    logcat, "[bind]:*=PORT is deprecated and no longer does anything in this version of Lokinet");
                return;
            }

            log::warning(
                logcat, "[bind]:{} is deprecated: Please update your config to use [bind]:listen instead", key);

            // Otherwise you could have either `A.B.C.D=PORT` or `IFNAME=port`.  The latter was
            // almost never used, and so we only look for the format and error on the latter.
            if (listen_addr)
                throw std::runtime_error{
                    "Multiple listen addresses found.  If upgrading from an older lokinet, replace extra "
                    "[bind]:inbound=/IP= settings with a single [bind]:listen="};

            uint16_t port{0};

            quic::Address temp;
            try
            {
                if (!llarp::parse_int<uint16_t>(val, port))
                    throw std::runtime_error{"Could not parse port"};
                temp = quic::Address{std::string{key}, port};
            }
            catch (const std::exception&)
            {
                throw std::runtime_error{
                    "Invalid [bind] deprecated config item: {}={}. "
                    "Please replace with a [bind]:listen=... directive"_format(key, val)};
            }

            listen_addr = std::move(temp);

            log::warning(
                logcat,
                "[bind]:{0}={1} is deprecated; please replace with [bind] config entry: listen={0}:{1}",
                key,
                val);
        });
    }

    void ApiConfig::define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params)
    {
        conf.add_section_comments(
            "api",
            {
                "JSON API settings",
            });

        constexpr std::array DefaultRPCBind{
            Default{"tcp://127.0.0.1:1190"},
#ifndef _WIN32
            Default{"ipc://rpc.sock"},
#endif
        };

        conf.define_option<bool>(
            "api",
            "enabled",
            NotEmbedded,
            Default{params.type == config::Type::FullClient},
            assignment_acceptor(enable_rpc_server),
            Comment{
                "Determines whether or not the OMQ JSON API is enabled. By default this is enabled for clients, "
                "disabled for relays",
            });

        conf.define_option<std::string>(
            "api",
            "bind",
            NotEmbedded,
            DefaultRPCBind,
            MultiValue,
            [this, first = true](std::string arg) mutable {
                if (first)
                {
                    rpc_bind_addrs.clear();
                    first = false;
                }
                if (arg.find("://") == std::string::npos)
                {
                    arg = "tcp://" + arg;
                }
                rpc_bind_addrs.push_back(std::move(arg));
            },
            Comment{
                "IP addresses and ports to bind to.",
                "Recommend localhost-only for security purposes.",
            });

        conf.define_option<std::string>("api", "authkey", Deprecated);

        // TODO: this was from pre-refactor:
        // TODO: add pubkey to whitelist
    }

    void LokidConfig::define_config_options(ConfigDefinition& conf, const ConfigGenParameters&)
    {
        conf.add_section_comments(
            "lokid",
            {
                "Settings for communicating with oxend",
            });

        conf.define_option<bool>(
            "lokid",
            "disable-testing",
            Default{false},
            Hidden,
            RelayOnly,
            Comment{"Development option: set to true to disable reachability testing when using", "testnet"},
            assignment_acceptor(disable_testing));

        conf.define_option<std::string>(
            "lokid",
            "rpc",
            RelayOnly,
            Required,
            Comment{
                "oxenmq control address for for communicating with oxend. Depends on oxend's",
                "lmq-local-control configuration option. By default this value should be",
                "ipc://OXEND-DATA-DIRECTORY/oxend.sock, such as:",
                "    rpc=ipc:///var/lib/oxen/oxend.sock",
                "    rpc=ipc:///home/USER/.oxen/oxend.sock",
                "but can use (non-default) TCP if oxend is configured that way:",
                "    rpc=tcp://127.0.0.1:5678",
            },
            [this](std::string arg) {
#ifndef LOKINET_EMBEDDED_ONLY
                oxenmq::address test_valid{arg};
#endif
                rpc_addr = std::move(arg);
            });

        // Deprecated options:
        conf.define_option<std::string>("lokid", "jsonrpc", RelayOnly, Hidden, [](std::string arg) {
            if (arg.empty())
                return;
            throw std::invalid_argument(
                "the [lokid]:jsonrpc option is no longer supported; please use the [lokid]:rpc config "
                "option instead with oxend's lmq-local-control address -- typically a value such as "
                "rpc=ipc:///var/lib/oxen/oxend.sock or rpc=ipc:///home/snode/.oxen/oxend.sock");
        });
        conf.define_option<bool>("lokid", "enabled", RelayOnly, Deprecated);
        conf.define_option<std::string>("lokid", "username", Deprecated);
        conf.define_option<std::string>("lokid", "password", Deprecated);
        conf.define_option<std::string>("lokid", "service-node-seed", Deprecated);
    }

    void BootstrapConfig::define_config_options(ConfigDefinition& conf, const ConfigGenParameters&)
    {
        conf.add_section_comments(
            "bootstrap",
            {
                "Configure nodes that will bootstrap us onto the network",
            });

        conf.define_option<std::string>(
            "bootstrap",
            "add-node",
            MultiValue,
            Comment{
                "Specify a bootstrap file containing a list of signed RelayContacts of service nodes",
                "which can act as a bootstrap. Can be specified multiple times. If set this overrides",
                "the built-in seed node list.",
            },
            [this](std::string arg) {
                if (arg.empty())
                    throw std::invalid_argument("cannot use empty filename as bootstrap");

                files.emplace_back(std::move(arg));

                if (not exists(files.back()))
                    throw std::invalid_argument("file does not exist: " + arg);
            });
    }

    void LoggingConfig::define_config_options(ConfigDefinition& conf, const ConfigGenParameters& params)
    {
        conf.add_section_comments(
            "logging",
            {
                "Logging settings",
            });

        conf.define_option<std::string>(
            "logging",
            "type",
            Default{
                params.type == config::Type::EmbeddedClient      ? "none"
                    : platform::is_android or platform::is_apple ? "system"
                                                                 : "print"},
            [this](std::string arg) {
                if (arg == "none")
                    type = std::nullopt;
                else
                    type = log::type_from_string(arg);
            },
            Comment{
                "Log type (format). Valid options are:",
                "  print - print logs to standard output",
                "  system - logs directed to the system logger (syslog/eventlog/etc.)",
                "  file - plaintext formatting to a file",
                (params.type == config::Type::EmbeddedClient ? "  none - do not reset the logging system (for embedded "
                                                               "use with external oxen::logging)"
                                                             : ""),
            });

        conf.define_option<std::string>(
            "logging",
            "level",
            Default{
                params.type == config::Type::Relay            ? "warn"
                    : params.type == config::Type::FullClient ? "info"
                                                              : ""},
            [this](std::string arg) { levels = std::move(arg); },
            Comment{
                "Minimum log severity level to print. Logging below this level will be ignored.",
                "Can also be set to a comma-separated list of individual categories, such as:",
                "    *=warn, logcat123=debug",
                "",
                "Valid log levels, in ascending order, are:",
                "  trace, debug, info, warn, error, critical, off",
            });

        conf.define_option<std::string>(
            "logging",
            "file",
            Default{""},
            assignment_acceptor(file),
            Comment{
                "When using type=file this is the output filename.",
            });
    }

    void PathConfig::define_config_options(ConfigDefinition& conf, const ConfigGenParameters&)
    {
        conf.add_section_comments(
            "paths",
            {
                "Settings related to path selection such as number of hops and selection criteria",
            });

        conf.define_option<int>(
            "paths",
            "edge-connections",
            Default{CLIENT_ROUTER_CONNECTIONS},
            ClientOnly,
            Comment{
                "Minimum number of routers lokinet client will attempt to maintain direct (i.e. \"edge\")",
                "connections to.  All paths will start through one of these edges.",
                "",
                "Lokinet may use more than this number of edges in single-hop connection mode",
                "(see [paths]:client-hops) and may use fewer connections if limited by [paths]:strict-edge."},
            lower_bounded_assignment_acceptor(edge_connections, 1, "[paths]:edge-connections"));

        conf.define_option<int>(
            "paths",
            "outbound-paths",
            ClientOnly,
            Default{2},
            Comment{
                "Number of paths to maintain per active outbound connection to a remote client or relay.",
                "Only one path is actively used at a time, but others are used for regular path rotation",
                "and as a fallback for path failure.",
                "",
                "Note that this value applies to EACH outbound connection separately: if you have active",
                "connections to 5 clients and 3 snodes, lokinet will maintain 16 outbound paths (at the",
                "default setting of 2).",
                "",
                "Setting this value to 1 is allowed, but will result in brief periods of packet loss",
                "whenever paths expire due to the lack of allowed backup path.",
            },
            bounded_assignment_acceptor(outbound_paths, 1, 4, "[paths]:outbound-paths"));

        conf.define_option<int>(
            "paths",
            "client-hops",
            ClientOnly,
            Default{3},
            Comment{
                "Number of hops to use when establishing a connection to a service node relay to",
                "communicate through that relay to another client.",
                "",
                "The overall number of hops to the remote client is this value PLUS the number of inbound",
                "hops the other client has configured for their inbound hops (via [paths]:inbound-hops).",
                "",
                "Setting this value to 1 puts lokinet into single-hop mode for the connection from this",
                "client to the aligned pivot router, which potentially weakens connection privacy as",
                "your public IP will be observable to any service node listed as a pivot for any remote",
                "client that you connect to."},
            bounded_assignment_acceptor(client_hops, 1, path::BUILD_LENGTH, "[paths]:client-hops"));

        conf.define_option<int>(
            "paths",
            "relay-hops",
            ClientOnly,
            Comment{
                "Number of hops to use when establishing a connection to talk to a service node.",
                "",
                "A value of 1 results in establishing direct connection to the snode (i.e. only",
                "encryption but no onion routing); 2 would select one intermediate snode to onion",
                "route through; 4 uses three intermediates, and so on, up to the maximum of 8.",
                "",
                "Additional hops increases privacy but also increase latency and reduces network",
                "performance through the path.",
                "",
                "If not set, this default to one greater than the value of [paths]:client-hops.",
                "",
                "Setting this value to 1 puts lokinet into single-hop mode for the connection from this",
                "client to service node (i.e. `.snode` addresses) which potentially weakens connection",
                "privacy as any service nodes you connect to will be able to observe your public IP."},
            bounded_assignment_acceptor(relay_hops_, 1, path::BUILD_LENGTH, "[paths]:relay-hops"));

        conf.define_option<int>(
            "paths",
            "inbound-paths",
            ClientOnly,
            Default{4},
            Comment{
                "Number of local paths that Lokinet maintains for both network reachability (i.e. remote",
                "clients connecting to this instance) and network communication such as looking up",
                "client lto maintain for network reachability and for general network requests",
                "",
                "This value does NOT apply to paths that are built to reach external clients or relays.",
            },
            bounded_assignment_acceptor(inbound_paths, 1, 10, "[paths]:local-paths"));

        conf.define_option<int>(
            "paths",
            "inbound-hops",
            ClientOnly,
            Comment{
                "Number of hops to use for inbound and general request connections (see [paths]:inbound-paths).",
                "",
                "When a remote Lokinet is connecting to this instance, this controls the path length of the",
                "local side of the full client-to-client path (i.e. from the common relay \"pivot\" to this",
                "Lokinet instance).",
                "",
                "If not set, this value defaults to the same value as [paths]:client-hops.",
            },
            bounded_assignment_acceptor(inbound_hops_, 1, path::BUILD_LENGTH, "[paths]:inbound-hops"));

        conf.define_option<int>(
            "paths",
            "unique-range-size",
            Default{24},
            ClientOnly,
            [this](int arg) {
                if (arg > 32 or (arg < 4 and arg != 0))
                    throw std::invalid_argument{"[paths]:unique-range-size must be between 4 and 32, or 0"};

                unique_hop_netmask = static_cast<uint8_t>(arg);
            },
            Comment{
                "Netmask for router path selection; each router must be from a distinct IPv4 subnet",
                "of the given size.  Defaults to 24.",
                "",
                "For instance, setting this to 16 selects routers for each path that have distinct",
                "x.y.*.* IP addresses; 32 merely requires that each router have a unique IP.  Setting",
                "this to 0 disables IP uniqueness entirely (i.e. paths can be selected that go through",
                "different Lokinet routers on the same IP)",
            });

        conf.define_option<std::chrono::seconds>(
            "paths",
            "acceptable-expiry",
            Default{300s},
            ClientOnly,
            Comment{
                "The minimum expiry time a path/pivot must have for it to be eligible when switching",
                "to a new path.  Inactive paths older than this will be replaced with new paths.",
            },
            bounded_assignment_acceptor(acceptable_expiry, 0s, path::MAX_LIFETIME / 2, "acceptable-expiry"));

        conf.define_option<std::chrono::seconds>(
            "paths",
            "min-expiry",
            Default{60s},
            ClientOnly,
            Comment{
                "The minimum allowed path/pivot expiry time (in seconds) of a currently active outbound path.",
                "When an active path reaches an expiry less than this value then the path to the remote will",
                "be rotated immediately to use a newer path.",
                "",
                "This value cannot be larger than acceptable-expiry.",
            },
            bounded_assignment_acceptor(min_expiry, 0s, path::MAX_LIFETIME / 2, "min-expiry"));

        conf.define_option<std::chrono::milliseconds>(
            "paths",
            "build-timeout",
            ClientOnly,
            Default{10s},
            Comment{
                "How long to wait for a session or path to establish before timing out the attempt.",
                "Value is in seconds, or milliseconds with an ms suffix (e.g. 2500ms).",
            },
            bounded_assignment_acceptor(build_timeout, 1ms, 1min, "[paths]:build-timeout"));

        conf.add_options_validator([this] {
            if (min_expiry > acceptable_expiry)
                throw std::invalid_argument{"[paths]:min-expiry cannot be longer than [paths]:acceptable-expiry"};
        });

        conf.define_option<std::chrono::seconds>(
            "paths",
            "ping-interval",
            Default{5s},
            ClientOnly,
            Comment{"How frequently to send pings along built paths to test that they are still alive."},
            lower_bounded_assignment_acceptor(ping_interval, 1s, "[paths]:ping-interval"));

        conf.define_option<int>(
            "paths",
            "max-missed-pings",
            Default{5},
            ClientOnly,
            Comment{
                "The maximum number of consecutive missed pings (see ping-interval) allowed for a path.  If a path",
                "misses more than this, the path will be considered to have died and be replaced."},
            lower_bounded_assignment_acceptor(max_missed_pings, 0, "[paths]:max-missed-pings"));

#ifdef LOKINET_DEBUG_PATH_SEED
        conf.define_option<uint64_t>(
            "paths", "debug-path-seed", ClientOnly, Hidden, assignment_acceptor(debug_path_seed));
#endif

        conf.define_option<std::string>(
            "paths",
            "strict-edge",
            ClientOnly,
            MultiValue,
            [this](std::string value) {
                RouterID router;
                if (value.size() == 64 && oxenc::is_hex(value))
                    oxenc::from_hex(value.begin(), value.end(), router.begin());
                else if (not router.from_relay_address(value))
                    throw std::invalid_argument{"[paths]:strict-edge: Invalid .snode pubkey: {}"_format(value)};

                if (not strict_edges.insert(router).second)
                    throw std::invalid_argument{
                        "[paths]:strict-edge: Duplicate strict connect .snode value: {}"_format(value)};
            },
            Comment{
                R"(List of service node public keys of "edge" nodes (also known as "first hops") that)",
                "Lokinet will exclusively use when establishing paths through the network.  You can use",
                "this to always use closer (i.e. lower latency) first hops, or to limit which network",
                "nodes see connections from your IP address.",
                "",
                "Public keys can be provided either in native lokinet address format (ADDR.snode), or using",
                "the 64-character hexademical pubkey notation common used for Session service nodes.",
                "Specify this option multiple times to specify multiple allowed edge nodes.",
                "",
                "Note that only registered service node pubkeys will be used, and so connectivity will be",
                "lost entirely if all of the listed pubkeys are or become deregistered.",
                "",
                "This option is incompatible with single-hop outbound path mode (see `[paths]:client-hops`",
                "and `[paths]:relay-hops`).",
                "",
                "Note that if bootstrapping is needed a connection will be made to the configured bootstrap",
                "nodes to obtain an initial router list.  See [bootstrap]:add-node if you want to also",
                "override the nodes used for bootstrapping."});

        conf.add_options_validator([this] {
            if (strict_edges.empty())
                return;
            if (client_hops == 1)
                throw std::invalid_argument{
                    "[paths]:strict-edge cannot be used with [paths]:client-hops=1 single hop mode"};
            if (relay_hops_ and *relay_hops_ == 1)
                throw std::invalid_argument{
                    "[paths]:strict-edge cannot be used with [paths]:relay-hops=1 single hop mode"};
        });

        conf.define_option<std::string>(
            "paths",
            "blacklist-snode",
            ClientOnly,
            MultiValue,
            Comment{
                "Adds a lokinet relay `.snode` address to the list of relays to avoid when",
                "connecting to edges or building paths. Can be specified multiple times.",
            },
            [this](std::string arg) {
                RouterID id;
                if (not id.from_relay_address(arg))
                    throw std::invalid_argument{"Invalid RouterID: {}"_format(arg)};

                auto itr = snode_blacklist.emplace(std::move(id));
                if (not itr.second)
                    throw std::invalid_argument{"Duplicate blacklist-snode: {}"_format(arg)};
            });

#ifdef WITH_GEOIP
        conf.defineOption<std::string>(
            "paths",
            "exclude-country",
            ClientOnly,
            MultiValue,
            [this](std::string arg) { m_ExcludeCountries.emplace(lowercase_ascii_string(std::move(arg))); },
            Comment{
                "Exclude a country given its 2 letter country code from being used in path builds.",
                "For example:",
                "    exclude-country=DE",
                "would avoid building paths through routers with IPs in Germany.",
                "This option can be specified multiple times to exclude multiple countries",
                "Note that this option does not affect the final relay or pivot in outgoing paths",
            });
#endif
    }

    std::unique_ptr<ConfigGenParameters> Config::make_gen_params() const
    {
        auto cgp = std::make_unique<ConfigGenParameters>();
        cgp->default_data_dir = data_dir;
        cgp->type = type;
        return cgp;
    }

    Config::Config(config::Type type, std::filesystem::path conf_file) : data_dir{conf_file.parent_path()}, type{type}
    {
        auto ini = util::file_to_string(conf_file);
        load_config_data(std::move(ini), std::move(conf_file));
    }

    Config::Config(config::Type type, std::string ini, std::filesystem::path default_data_dir)
        : data_dir{std::move(default_data_dir)}, type{type}
    {
        load_config_data(std::move(ini));
    }

    static std::filesystem::path overrides_dir(const std::filesystem::path& datadir) { return datadir / "conf.d"; }

    void Config::save()
    {
        const auto overridesDir = overrides_dir(data_dir);
        if (not exists(overridesDir))
            create_directories(overridesDir);
        parser.save();
    }

    void Config::override(std::string section, std::string key, std::string value)
    {
        parser.add_override(overrides_dir(data_dir) / "overrides.ini", section, key, value);
    }

    void Config::load_overrides(ConfigDefinition& conf) const
    {
        ConfigParser parser;
        const auto overridesDir = overrides_dir(data_dir);
        if (exists(overridesDir))
        {
            for (const auto& f : std::filesystem::directory_iterator{overridesDir})
            {
                if (not f.is_regular_file() or f.path().extension() != ".ini")
                    continue;
                ConfigParser parser;
                try
                {
                    parser.load_file(f.path());
                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error{"Failed to load config file {}: {}"_format(f.path().string(), e.what())};
                }

                parser.iter_all_sections([&](std::string_view section, const SectionValues& values) {
                    for (const auto& [k, v] : values)
                        conf.add_config_value(section, k, v);
                });
            }
        }
    }

    void Config::add_default(std::string section, std::string key, std::string val)
    {
        additional.emplace_back(std::array<std::string, 3>{section, key, val});
    }

    void Config::load_config_data(std::string ini, std::optional<std::filesystem::path> filename)
    {
#ifdef LOKINET_EMBEDDED_ONLY
        if (type != Type::EmbeddedClient)
            throw std::runtime_error{
                "This lokinet build only supports embedded clients, not {}"_format(to_string(type))};
#endif
        auto params = make_gen_params();
        ConfigDefinition conf{type};
        add_backcompat_opts(conf);
        init_config(conf, *params);

        for (const auto& item : additional)
        {
            conf.add_config_value(item[0], item[1], item[2]);
        }

        parser.clear();

        if (filename)
            parser.set_filename(*filename);
        else
            parser.set_filename(std::filesystem::path{});

        parser.load_from_str(std::move(ini));

        parser.iter_all_sections([&](std::string_view section, const SectionValues& values) {
            for (const auto& pair : values)
            {
                conf.add_config_value(section, pair.first, pair.second);
            }
        });

        load_overrides(conf);

        conf.process();
    }

    void Config::init_config(ConfigDefinition& conf, const ConfigGenParameters& params)
    {
        router.define_config_options(conf, params);
        exit.define_config_options(conf, params);
        network.define_config_options(conf, params);
        paths.define_config_options(conf, params);
        dns.define_config_options(conf, params);
        links.define_config_options(conf, params);
        api.define_config_options(conf, params);
        lokid.define_config_options(conf, params);
        bootstrap.define_config_options(conf, params);
        logging.define_config_options(conf, params);
    }

    void Config::add_backcompat_opts(ConfigDefinition& conf)
    {
        // These config sections don't exist anymore:

        conf.define_option<std::string>("system", "user", Deprecated);
        conf.define_option<std::string>("system", "group", Deprecated);
        conf.define_option<std::string>("system", "pidfile", Deprecated);

        conf.define_option<std::string>("netdb", "dir", Deprecated);

        conf.define_option<std::string>("metrics", "json-metrics-path", Deprecated);
    }

    void ensure_config(std::filesystem::path dataDir, std::filesystem::path confFile, bool overwrite, config::Type type)
    {
        // fail to overwrite if not instructed to do so
        if (exists(confFile) && !overwrite)
        {
            log::info(logcat, "Config file already exists; NOT creating new config");
            return;
        }

        const auto parent = confFile.parent_path();

        // create parent dir if it doesn't exist
        if ((not parent.empty()) and (not exists(parent)))
        {
            create_directory(parent);
        }

        log::info(logcat, "Attempting to create config file for {} at file path:{}", to_string(type), confFile);

        llarp::Config config{type, "", dataDir};
        auto confStr = config.generate_config_base();

        try
        {
            util::buffer_to_file(confFile, confStr);
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error{"Failed to write config data to {}: {}"_format(confFile, e.what())};
        }

        log::info(logcat, "Generated new config (path: {})", confFile);
    }

    std::string Config::generate_config_base()
    {
        auto params = make_gen_params();

        llarp::ConfigDefinition def{type};
        init_config(def, *params);

        return def.generate_ini_config(true);
    }

}  // namespace llarp
