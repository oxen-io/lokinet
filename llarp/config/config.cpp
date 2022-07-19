#include <chrono>
#include "config.hpp"

#include "config/definition.hpp"
#include "ini.hpp"
#include <llarp/constants/files.hpp>
#include <llarp/constants/platform.hpp>
#include <llarp/constants/version.hpp>
#include <llarp/net/net.hpp>
#include <llarp/net/ip.hpp>
#include <llarp/router_contact.hpp>
#include <stdexcept>
#include <llarp/util/fs.hpp>
#include <llarp/util/formattable.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/mem.hpp>
#include <llarp/util/str.hpp>

#include <llarp/service/name.hpp>

#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>

namespace llarp
{
  // constants for config file default values
  constexpr int DefaultMinConnectionsForRouter = 6;
  constexpr int DefaultMaxConnectionsForRouter = 60;

  constexpr int DefaultMinConnectionsForClient = 4;
  constexpr int DefaultMaxConnectionsForClient = 6;

  constexpr int DefaultPublicPort = 1090;

  using namespace config;

  void
  RouterConfig::defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params)
  {
    constexpr Default DefaultJobQueueSize{1024 * 8};
    constexpr Default DefaultWorkerThreads{0};
    constexpr Default DefaultBlockBogons{true};

    conf.defineOption<int>(
        "router", "job-queue-size", DefaultJobQueueSize, Hidden, [this](int arg) {
          if (arg < 1024)
            throw std::invalid_argument("job-queue-size must be 1024 or greater");

          m_JobQueueSize = arg;
        });

    conf.defineOption<std::string>(
        "router",
        "netid",
        Default{llarp::DEFAULT_NETID},
        Comment{
            "Network ID; this is '"s + llarp::DEFAULT_NETID + "' for mainnet, 'gamma' for testnet.",
        },
        [this](std::string arg) {
          if (arg.size() > NetID::size())
            throw std::invalid_argument{
                fmt::format("netid is too long, max length is {}", NetID::size())};

          m_netId = std::move(arg);
        });

    int minConnections =
        (params.isRelay ? DefaultMinConnectionsForRouter : DefaultMinConnectionsForClient);
    conf.defineOption<int>(
        "router",
        "min-connections",
        Default{minConnections},
        Comment{
            "Minimum number of routers lokinet will attempt to maintain connections to.",
        },
        [=](int arg) {
          if (arg < minConnections)
            throw std::invalid_argument{
                fmt::format("min-connections must be >= {}", minConnections)};

          m_minConnectedRouters = arg;
        });

    int maxConnections =
        (params.isRelay ? DefaultMaxConnectionsForRouter : DefaultMaxConnectionsForClient);
    conf.defineOption<int>(
        "router",
        "max-connections",
        Default{maxConnections},
        Comment{
            "Maximum number (hard limit) of routers lokinet will be connected to at any time.",
        },
        [=](int arg) {
          if (arg < maxConnections)
            throw std::invalid_argument{
                fmt::format("max-connections must be >= {}", maxConnections)};

          m_maxConnectedRouters = arg;
        });

    conf.defineOption<std::string>("router", "nickname", Hidden, AssignmentAcceptor(m_nickname));

    conf.defineOption<fs::path>(
        "router",
        "data-dir",
        Default{params.defaultDataDir},
        Comment{
            "Optional directory for containing lokinet runtime data. This includes generated",
            "private keys.",
        },
        [this](fs::path arg) {
          if (arg.empty())
            throw std::invalid_argument("[router]:data-dir is empty");
          if (not fs::exists(arg))
            throw std::runtime_error{
                fmt::format("Specified [router]:data-dir {} does not exist", arg)};

          m_dataDir = std::move(arg);
        });

    conf.defineOption<std::string>(
        "router",
        "public-ip",
        RelayOnly,
        Comment{
            "For complex network configurations where the detected IP is incorrect or non-public",
            "this setting specifies the public IP at which this router is reachable. When",
            "provided the public-port option must also be specified.",
        },
        [this](std::string arg) {
          if (arg.empty())
            return;
          nuint32_t addr{};
          if (not addr.FromString(arg))
            throw std::invalid_argument{fmt::format("{} is not a valid IPv4 address", arg)};

          if (IsIPv4Bogon(addr))
            throw std::invalid_argument{
                fmt::format("{} is not a publicly routable ip address", addr)};

          m_PublicIP = addr;
        });

    conf.defineOption<std::string>("router", "public-address", Hidden, [](std::string) {
      throw std::invalid_argument{
          "[router]:public-address option no longer supported, use [router]:public-ip and "
          "[router]:public-port instead"};
    });

    conf.defineOption<int>(
        "router",
        "public-port",
        RelayOnly,
        Default{DefaultPublicPort},
        Comment{
            "When specifying public-ip=, this specifies the public UDP port at which this lokinet",
            "router is reachable. Required when public-ip is used.",
        },
        [this](int arg) {
          if (arg <= 0 || arg > std::numeric_limits<uint16_t>::max())
            throw std::invalid_argument("public-port must be >= 0 and <= 65536");
          m_PublicPort = ToNet(huint16_t{static_cast<uint16_t>(arg)});
        });

    conf.defineOption<int>(
        "router",
        "worker-threads",
        DefaultWorkerThreads,
        Comment{
            "The number of threads available for performing cryptographic functions.",
            "The minimum is one thread, but network performance may increase with more.",
            "threads. Should not exceed the number of logical CPU cores.",
            "0 means use the number of logical CPU cores detected at startup.",
        },
        [this](int arg) {
          if (arg < 0)
            throw std::invalid_argument("worker-threads must be >= 0");

          m_workerThreads = arg;
        });

    // Hidden option because this isn't something that should ever be turned off occasionally when
    // doing dev/testing work.
    conf.defineOption<bool>(
        "router", "block-bogons", DefaultBlockBogons, Hidden, AssignmentAcceptor(m_blockBogons));

    constexpr auto relative_to_datadir =
        "An absolute path is used as-is, otherwise relative to 'data-dir'.";

    conf.defineOption<std::string>(
        "router",
        "contact-file",
        RelayOnly,
        Default{llarp::our_rc_filename},
        AssignmentAcceptor(m_routerContactFile),
        Comment{
            "Filename in which to store the router contact file",
            relative_to_datadir,
        });

    conf.defineOption<std::string>(
        "router",
        "encryption-privkey",
        RelayOnly,
        Default{llarp::our_enc_key_filename},
        AssignmentAcceptor(m_encryptionKeyFile),
        Comment{
            "Filename in which to store the encryption private key",
            relative_to_datadir,
        });

    conf.defineOption<std::string>(
        "router",
        "ident-privkey",
        RelayOnly,
        Default{llarp::our_identity_filename},
        AssignmentAcceptor(m_identityKeyFile),
        Comment{
            "Filename in which to store the identity private key",
            relative_to_datadir,
        });

    conf.defineOption<std::string>(
        "router",
        "transport-privkey",
        RelayOnly,
        Default{llarp::our_transport_key_filename},
        AssignmentAcceptor(m_transportKeyFile),
        Comment{
            "Filename in which to store the transport private key.",
            relative_to_datadir,
        });

    // Deprecated options:

    // these weren't even ever used!
    conf.defineOption<std::string>("router", "max-routers", Deprecated);
    conf.defineOption<std::string>("router", "min-routers", Deprecated);

    // TODO: this may have been a synonym for [router]worker-threads
    conf.defineOption<std::string>("router", "threads", Deprecated);
    conf.defineOption<std::string>("router", "net-threads", Deprecated);

    m_isRelay = params.isRelay;
  }

  void
  NetworkConfig::defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params)
  {
    (void)params;

    static constexpr Default ProfilingValueDefault{true};
    static constexpr Default SaveProfilesDefault{true};
    static constexpr Default ReachableDefault{true};
    static constexpr Default HopsDefault{4};
    static constexpr Default PathsDefault{6};
    static constexpr Default IP6RangeDefault{"fd00::"};

    conf.defineOption<std::string>(
        "network", "type", Default{"tun"}, Hidden, AssignmentAcceptor(m_endpointType));

    conf.defineOption<bool>(
        "network",
        "save-profiles",
        SaveProfilesDefault,
        Hidden,
        AssignmentAcceptor(m_saveProfiles));

    conf.defineOption<bool>(
        "network",
        "profiling",
        ProfilingValueDefault,
        Hidden,
        AssignmentAcceptor(m_enableProfiling));

    conf.defineOption<std::string>("network", "profiles", Deprecated);

    conf.defineOption<std::string>(
        "network",
        "strict-connect",
        ClientOnly,
        MultiValue,
        [this](std::string value) {
          RouterID router;
          if (not router.FromString(value))
            throw std::invalid_argument{"bad snode value: " + value};
          if (not m_strictConnect.insert(router).second)
            throw std::invalid_argument{"duplicate strict connect snode: " + value};
        },
        Comment{
            "Public key of a router which will act as a pinned first-hop. This may be used to",
            "provide a trusted router (consider that you are not fully anonymous with your",
            "first hop).",
        });

    conf.defineOption<std::string>(
        "network",
        "keyfile",
        ClientOnly,
        AssignmentAcceptor(m_keyfile),
        Comment{
            "The private key to persist address with. If not specified the address will be",
            "ephemeral.",
        });

    conf.defineOption<std::string>(
        "network",
        "auth",
        ClientOnly,
        Comment{
            "Set the endpoint authentication mechanism.",
            "none/whitelist/lmq/file",
        },
        [this](std::string arg) {
          if (arg.empty())
            return;
          m_AuthType = service::ParseAuthType(arg);
        });

    conf.defineOption<std::string>(
        "network",
        "auth-lmq",
        ClientOnly,
        AssignmentAcceptor(m_AuthUrl),
        Comment{
            "lmq endpoint to talk to for authenticating new sessions",
            "ipc:///var/lib/lokinet/auth.socket",
            "tcp://127.0.0.1:5555",
        });

    conf.defineOption<std::string>(
        "network",
        "auth-lmq-method",
        ClientOnly,
        Default{"llarp.auth"},
        Comment{
            "lmq function to call for authenticating new sessions",
            "llarp.auth",
        },
        [this](std::string arg) {
          if (arg.empty())
            return;
          m_AuthMethod = std::move(arg);
        });

    conf.defineOption<std::string>(
        "network",
        "auth-whitelist",
        ClientOnly,
        MultiValue,
        Comment{
            "manually add a remote endpoint by .loki address to the access whitelist",
        },
        [this](std::string arg) {
          service::Address addr;
          if (not addr.FromString(arg))
            throw std::invalid_argument{fmt::format("bad loki address: {}", arg)};
          m_AuthWhitelist.emplace(std::move(addr));
        });

    conf.defineOption<fs::path>(
        "network",
        "auth-file",
        ClientOnly,
        MultiValue,
        Comment{
            "Read auth tokens from file to accept endpoint auth",
            "Can be provided multiple times",
        },
        [this](fs::path arg) {
          if (not fs::exists(arg))
            throw std::invalid_argument{
                fmt::format("cannot load auth file {}: file does not exist", arg)};
          m_AuthFiles.emplace(std::move(arg));
        });
    conf.defineOption<std::string>(
        "network",
        "auth-file-type",
        ClientOnly,
        Comment{
            "How to interpret the contents of an auth file.",
            "Possible values: hashes, plaintext",
        },
        [this](std::string arg) { m_AuthFileType = service::ParseAuthFileType(std::move(arg)); });

    conf.defineOption<std::string>(
        "network",
        "auth-static",
        ClientOnly,
        MultiValue,
        Comment{
            "Manually add a static auth code to accept for endpoint auth",
            "Can be provided multiple times",
        },
        [this](std::string arg) { m_AuthStaticTokens.emplace(std::move(arg)); });

    conf.defineOption<bool>(
        "network",
        "reachable",
        ClientOnly,
        ReachableDefault,
        AssignmentAcceptor(m_reachable),
        Comment{
            "Determines whether we will publish our snapp's introset to the DHT.",
        });

    conf.defineOption<int>(
        "network",
        "hops",
        HopsDefault,
        Comment{
            "Number of hops in a path. Min 1, max 8.",
        },
        [this](int arg) {
          if (arg < 1 or arg > 8)
            throw std::invalid_argument("[endpoint]:hops must be >= 1 and <= 8");
          m_Hops = arg;
        });

    conf.defineOption<int>(
        "network",
        "paths",
        ClientOnly,
        PathsDefault,
        Comment{
            "Number of paths to maintain at any given time.",
        },
        [this](int arg) {
          if (arg < 3 or arg > 8)
            throw std::invalid_argument("[endpoint]:paths must be >= 3 and <= 8");
          m_Paths = arg;
        });

    conf.defineOption<bool>(
        "network",
        "exit",
        ClientOnly,
        Default{false},
        AssignmentAcceptor(m_AllowExit),
        Comment{
            "Whether or not we should act as an exit node. Beware that this increases demand",
            "on the server and may pose liability concerns. Enable at your own risk.",
        });

    conf.defineOption<std::string>(
        "network",
        "owned-range",
        MultiValue,
        Comment{
            "When in exit mode announce we allow a private range in our introset"
            "exmaple:",
            "owned-range=10.0.0.0/24",
        },
        [this](std::string arg) {
          IPRange range;
          if (not range.FromString(arg))
            throw std::invalid_argument{"bad ip range: '" + arg + "'"};
          m_OwnedRanges.insert(range);
        });

    conf.defineOption<std::string>(
        "network",
        "traffic-whitelist",
        MultiValue,
        Comment{
            "List of ip traffic whitelist, anything not specified will be dropped by us."
            "examples:",
            "tcp for all tcp traffic regardless of port",
            "0x69 for all packets using ip protocol 0x69"
            "udp/53 for udp port 53",
            "tcp/smtp for smtp port",
        },
        [this](std::string arg) {
          if (not m_TrafficPolicy)
            m_TrafficPolicy = net::TrafficPolicy{};

          // this will throw on error
          m_TrafficPolicy->protocols.emplace(arg);
        });

    conf.defineOption<std::string>(
        "network",
        "exit-node",
        ClientOnly,
        MultiValue,
        Comment{
            "Specify a `.loki` address and an optional ip range to use as an exit broker.",
            "Example:",
            "exit-node=whatever.loki # maps all exit traffic to whatever.loki",
            "exit-node=stuff.loki:100.0.0.0/24 # maps 100.0.0.0/24 to stuff.loki",
        },
        [this](std::string arg) {
          if (arg.empty())
            return;
          service::Address exit;
          IPRange range;
          const auto pos = arg.find(":");
          if (pos == std::string::npos)
          {
            range.FromString("::/0");
          }
          else if (not range.FromString(arg.substr(pos + 1)))
          {
            throw std::invalid_argument("[network]:exit-node invalid ip range for exit provided");
          }
          if (pos != std::string::npos)
          {
            arg = arg.substr(0, pos);
          }

          if (service::NameIsValid(arg))
          {
            m_LNSExitMap.Insert(range, arg);
            return;
          }

          if (arg != "null" and not exit.FromString(arg))
          {
            throw std::invalid_argument{fmt::format("[network]:exit-node bad address: {}", arg)};
          }
          m_ExitMap.Insert(range, exit);
        });

    conf.defineOption<std::string>(
        "network",
        "exit-auth",
        ClientOnly,
        MultiValue,
        Comment{
            "Specify an optional authentication code required to use a non-public exit node.",
            "For example:",
            "    exit-auth=myfavouriteexit.loki:abc",
            "uses the authentication code `abc` whenever myfavouriteexit.loki is accessed.",
            "Can be specified multiple times to store codes for different exit nodes.",
        },
        [this](std::string arg) {
          if (arg.empty())
            return;
          service::Address exit;
          service::AuthInfo auth;
          const auto pos = arg.find(":");
          if (pos == std::string::npos)
          {
            throw std::invalid_argument(
                "[network]:exit-auth invalid format, expects "
                "exit-address.loki:auth-code-goes-here");
          }
          const auto exit_str = arg.substr(0, pos);
          auth.token = arg.substr(pos + 1);

          if (service::NameIsValid(exit_str))
          {
            m_LNSExitAuths.emplace(exit_str, auth);
            return;
          }

          if (not exit.FromString(exit_str))
          {
            throw std::invalid_argument("[network]:exit-auth invalid exit address");
          }
          m_ExitAuths.emplace(exit, auth);
        });

    conf.defineOption<bool>(
        "network",
        "auto-routing",
        ClientOnly,
        Default{true},
        Comment{
            "Enable / disable automatic route configuration.",
            "When this is enabled and an exit is used Lokinet will automatically configure "
            "operating system routes to route traffic through the exit node.",
            "This is enabled by default, but can be disabled to perform advanced exit routing "
            "configuration manually."},
        AssignmentAcceptor(m_EnableRoutePoker));

    conf.defineOption<bool>(
        "network",
        "blackhole-routes",
        ClientOnly,
        Default{true},
        Comment{
            "Enable / disable route configuration blackholes.",
            "When enabled lokinet will drop ip4 and ip6 not included in exit config.",
            "Enabled by default."},
        AssignmentAcceptor(m_BlackholeRoutes));

    conf.defineOption<std::string>(
        "network",
        "ifname",
        Comment{
            "Interface name for lokinet traffic. If unset lokinet will look for a free name",
            "lokinetN, starting at 0 (e.g. lokinet0, lokinet1, ...).",
        },
        AssignmentAcceptor(m_ifname));

    conf.defineOption<std::string>(
        "network",
        "ifaddr",
        Comment{
            "Local IP and range for lokinet traffic. For example, 172.16.0.1/16 to use",
            "172.16.0.1 for this machine and 172.16.x.y for remote peers. If omitted then",
            "lokinet will attempt to find an unused private range.",
        },
        [this](std::string arg) {
          if (not m_ifaddr.FromString(arg))
          {
            throw std::invalid_argument{fmt::format("[network]:ifaddr invalid value: '{}'", arg)};
          }
        });

    conf.defineOption<std::string>(
        "network",
        "ip6-range",
        ClientOnly,
        Comment{
            "For all ipv6 exit traffic you will use this as the base address bitwised or'd with "
            "the v4 address in use.",
            "To disable ipv6 set this to an empty value.",
            "!!! WARNING !!! Disabling ipv6 tunneling when you have ipv6 routes WILL lead to "
            "de-anonymization as lokinet will no longer carry your ipv6 traffic.",
        },
        IP6RangeDefault,
        [this](std::string arg) {
          if (arg.empty())
          {
            LogError(
                "!!! Disabling ipv6 tunneling when you have ipv6 routes WILL lead to "
                "de-anonymization as lokinet will no longer carry your ipv6 traffic !!!");
            m_baseV6Address = std::nullopt;
            return;
          }
          m_baseV6Address = huint128_t{};
          if (not m_baseV6Address->FromString(arg))
            throw std::invalid_argument{
                fmt::format("[network]:ip6-range invalid value: '{}'", arg)};
        });
    // TODO: could be useful for snodes in the future, but currently only implemented for clients:
    conf.defineOption<std::string>(
        "network",
        "mapaddr",
        ClientOnly,
        MultiValue,
        Comment{
            "Map a remote `.loki` address to always use a fixed local IP. For example:",
            "    mapaddr=whatever.loki:172.16.0.10",
            "maps `whatever.loki` to `172.16.0.10` instead of using the next available IP.",
            "The given IP address must be inside the range configured by ifaddr=",
        },
        [this](std::string arg) {
          if (arg.empty())
            return;
          huint128_t ip;
          service::Address addr;
          const auto pos = arg.find(":");
          if (pos == std::string::npos)
          {
            throw std::invalid_argument{fmt::format("[endpoint]:mapaddr invalid entry: {}", arg)};
          }
          std::string addrstr = arg.substr(0, pos);
          std::string ipstr = arg.substr(pos + 1);
          if (not ip.FromString(ipstr))
          {
            huint32_t ipv4;
            if (not ipv4.FromString(ipstr))
            {
              throw std::invalid_argument{fmt::format("[endpoint]:mapaddr invalid ip: {}", ipstr)};
            }
            ip = net::ExpandV4(ipv4);
          }
          if (not addr.FromString(addrstr))
          {
            throw std::invalid_argument{
                fmt::format("[endpoint]:mapaddr invalid addresss: {}", addrstr)};
          }
          if (m_mapAddrs.find(ip) != m_mapAddrs.end())
          {
            throw std::invalid_argument{
                fmt::format("[endpoint]:mapaddr ip already mapped: {}", ipstr)};
          }
          m_mapAddrs[ip] = addr;
        });

    conf.defineOption<std::string>(
        "network",
        "blacklist-snode",
        ClientOnly,
        MultiValue,
        Comment{
            "Adds a lokinet relay `.snode` address to the list of relays to avoid when",
            "building paths. Can be specified multiple times.",
        },
        [this](std::string arg) {
          RouterID id;
          if (not id.FromString(arg))
            throw std::invalid_argument{fmt::format("Invalid RouterID: {}", arg)};

          auto itr = m_snodeBlacklist.emplace(std::move(id));
          if (not itr.second)
            throw std::invalid_argument{fmt::format("Duplicate blacklist-snode: {}", arg)};
        });

    // TODO: support SRV records for routers, but for now client only
    conf.defineOption<std::string>(
        "network",
        "srv",
        ClientOnly,
        MultiValue,
        Comment{
            "Specify SRV Records for services hosted on the SNApp",
            "for more info see https://docs.loki.network/Lokinet/Guides/HostingSNApps/",
            "srv=_service._protocol priority weight port target.loki",
        },
        [this](std::string arg) {
          llarp::dns::SRVData newSRV;
          if (not newSRV.fromString(arg))
            throw std::invalid_argument{fmt::format("Invalid SRV Record string: {}", arg)};

          m_SRVRecords.push_back(std::move(newSRV));
        });

    conf.defineOption<int>(
        "network",
        "path-alignment-timeout",
        ClientOnly,
        Comment{
            "time in seconds how long to wait for a path to align to pivot routers",
            "if not provided a sensible default will be used",
        },
        [this](int val) {
          if (val <= 0)
            throw std::invalid_argument{
                "invalid path alignment timeout: " + std::to_string(val) + " <= 0"};
          m_PathAlignmentTimeout = std::chrono::seconds{val};
        });

    conf.defineOption<fs::path>(
        "network",
        "persist-addrmap-file",
        ClientOnly,
        Default{fs::path{params.defaultDataDir / "addrmap.dat"}},
        Comment{
            "persist mapped ephemeral addresses to a file",
            "on restart the mappings will be loaded so that ip addresses will not be mapped to a "
            "different address",
        },
        [this](fs::path arg) {
          if (arg.empty())
            throw std::invalid_argument("persist-addrmap-file cannot be empty");
          m_AddrMapPersistFile = arg;
        });

    // Deprecated options:
    conf.defineOption<std::string>("network", "enabled", Deprecated);
  }

  void
  DnsConfig::defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params)
  {
    (void)params;

    // Most non-linux platforms have loopback as 127.0.0.1/32, but linux uses 127.0.0.1/8 so that we
    // can bind to other 127.* IPs to avoid conflicting with something else that may be listening on
    // 127.0.0.1:53.
#ifdef __linux__
    constexpr Default DefaultDNSBind{"127.3.2.1:53"};
#else
    constexpr Default DefaultDNSBind{"127.0.0.1:53"};
#endif

    // Default, but if we get any upstream (including upstream=, i.e. empty string) we clear it
    constexpr Default DefaultUpstreamDNS{"9.9.9.10"};
    m_upstreamDNS.emplace_back(DefaultUpstreamDNS.val);
    if (!m_upstreamDNS.back().getPort())
      m_upstreamDNS.back().setPort(53);

    conf.defineOption<std::string>(
        "dns",
        "upstream",
        DefaultUpstreamDNS,
        MultiValue,
        Comment{
            "Upstream resolver(s) to use as fallback for non-loki addresses.",
            "Multiple values accepted.",
        },
        [=, first = true](std::string arg) mutable {
          if (first)
          {
            m_upstreamDNS.clear();
            first = false;
          }
          if (!arg.empty())
          {
            auto& entry = m_upstreamDNS.emplace_back(std::move(arg));
            if (!entry.getPort())
              entry.setPort(53);
          }
        });

    conf.defineOption<std::string>(
        "dns",
        "bind",
        DefaultDNSBind,
        Comment{
            "Address to bind to for handling DNS requests.",
        },
        [=](std::string arg) {
          m_bind = SockAddr{std::move(arg)};
          if (!m_bind.getPort())
            m_bind.setPort(53);
        });

    conf.defineOption<fs::path>(
        "dns",
        "add-hosts",
        ClientOnly,
        Comment{"Add a hosts file to the dns resolver", "For use with client side dns filtering"},
        [=](fs::path path) {
          if (path.empty())
            return;
          if (not fs::exists(path))
            throw std::invalid_argument{
                fmt::format("cannot add hosts file {} as it does not exist", path)};
          m_hostfiles.emplace_back(std::move(path));
        });

    // Ignored option (used by the systemd service file to disable resolvconf configuration).
    conf.defineOption<bool>(
        "dns",
        "no-resolvconf",
        ClientOnly,
        Comment{
            "Can be uncommented and set to 1 to disable resolvconf configuration of lokinet DNS.",
            "(This is not used directly by lokinet itself, but by the lokinet init scripts",
            "on systems which use resolveconf)",
        });
  }

  LinksConfig::LinkInfo
  LinksConfig::LinkInfoFromINIValues(std::string_view name, std::string_view value)
  {
    // we treat the INI k:v pair as:
    // k: interface name, * indicating outbound
    // v: a comma-separated list of values, an int indicating port (everything else ignored)
    //    this is somewhat of a backwards- and forwards-compatibility thing

    LinkInfo info;
    info.port = 0;
    info.addressFamily = AF_INET;

    if (name == "address")
    {
      const IpAddress addr{value};
      if (not addr.hasPort())
        throw std::invalid_argument("no port provided in link address");
      info.m_interface = addr.toHost();
      info.port = *addr.getPort();
    }
    else
    {
      info.m_interface = std::string{name};

      std::vector<std::string_view> splits = split(value, ",");
      for (std::string_view str : splits)
      {
        int asNum = std::atoi(str.data());
        if (asNum > 0)
          info.port = asNum;

        // otherwise, ignore ("future-proofing")
      }
    }

    return info;
  }

  void
  LinksConfig::defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params)
  {
    constexpr Default DefaultOutboundLinkValue{"0"};

    conf.addSectionComments(
        "bind",
        {
            "This section specifies network interface names and/or IPs as keys, and",
            "ports as values to control the address(es) on which Lokinet listens for",
            "incoming data.",
            "",
            "Examples:",
            "",
            "    eth0=1090",
            "    0.0.0.0=1090",
            "    1.2.3.4=1090",
            "",
            "The first bind to port 1090 on the network interface 'eth0'; the second binds",
            "to port 1090 on all local network interfaces; and the third example binds to",
            "port 1090 on the given IP address.",
            "",
            "If a private range IP address (or an interface with a private IP) is given, or",
            "if the 0.0.0.0 all-address IP is given then you must also specify the",
            "public-ip= and public-port= settings in the [router] section with a public",
            "address at which this router can be reached.",
            "",
            "Typically this section can be left blank: if no inbound bind addresses are",
            "configured then lokinet will search for a local network interface with a public",
            "IP address and use that (with port 1090).",
        });

    conf.defineOption<std::string>(
        "bind",
        "*",
        DefaultOutboundLinkValue,
        Comment{
            "Specify a source port for **outgoing** Lokinet traffic, for example if you want to",
            "set up custom firewall rules based on the originating port. Typically this should",
            "be left unset to automatically choose random source ports.",
        },
        [this](std::string arg) { m_OutboundLink = LinkInfoFromINIValues("*", arg); });

    if (params.isRelay)
    {
      if (std::string best_if; GetBestNetIF(best_if))
        m_InboundLinks.push_back(LinkInfoFromINIValues(best_if, std::to_string(DefaultPublicPort)));
    }
    conf.addUndeclaredHandler(
        "bind",
        [&, defaulted = true](
            std::string_view, std::string_view name, std::string_view value) mutable {
          if (defaulted)
          {
            m_InboundLinks.clear();  // Clear the default
            defaulted = false;
          }

          LinkInfo info = LinkInfoFromINIValues(name, value);

          if (info.port <= 0)
            throw std::invalid_argument{
                fmt::format("Invalid [bind] port specified on interface {}", name)};

          assert(name != "*");  // handled by defineOption("bind", "*", ...) above

          m_InboundLinks.emplace_back(std::move(info));
        });
  }

  void
  ConnectConfig::defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params)
  {
    (void)params;

    conf.addUndeclaredHandler(
        "connect", [this](std::string_view section, std::string_view name, std::string_view value) {
          fs::path file{value.begin(), value.end()};
          if (not fs::exists(file))
            throw std::runtime_error{fmt::format(
                "Specified bootstrap file {} specified in [{}]:{} does not exist",
                value,
                section,
                name)};

          routers.emplace_back(std::move(file));
          return true;
        });
  }

  void
  ApiConfig::defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params)
  {
    constexpr Default DefaultRPCBindAddr{"tcp://127.0.0.1:1190"};

    conf.defineOption<bool>(
        "api",
        "enabled",
        Default{not params.isRelay},
        AssignmentAcceptor(m_enableRPCServer),
        Comment{
            "Determines whether or not the LMQ JSON API is enabled. Defaults ",
        });

    conf.defineOption<std::string>(
        "api",
        "bind",
        DefaultRPCBindAddr,
        [this](std::string arg) {
          if (arg.empty())
          {
            arg = DefaultRPCBindAddr.val;
          }
          if (arg.find("://") == std::string::npos)
          {
            arg = "tcp://" + arg;
          }
          m_rpcBindAddr = std::move(arg);
        },
        Comment{
            "IP address and port to bind to.",
            "Recommend localhost-only for security purposes.",
        });

    conf.defineOption<std::string>("api", "authkey", Deprecated);

    // TODO: this was from pre-refactor:
    // TODO: add pubkey to whitelist
  }

  void
  LokidConfig::defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params)
  {
    (void)params;

    conf.defineOption<bool>(
        "lokid",
        "enabled",
        RelayOnly,
        Default{true},
        Comment{
            "Whether or not we should talk to lokid. Must be enabled for staked routers.",
        },
        AssignmentAcceptor(whitelistRouters));

    conf.defineOption<std::string>("lokid", "jsonrpc", RelayOnly, [](std::string arg) {
      if (arg.empty())
        return;
      throw std::invalid_argument(
          "the [lokid]:jsonrpc option is no longer supported; please use the [lokid]:rpc config "
          "option instead with lokid's lmq-local-control address -- typically a value such as "
          "rpc=ipc:///var/lib/loki/lokid.sock or rpc=ipc:///home/snode/.loki/lokid.sock");
    });

    conf.defineOption<std::string>(
        "lokid",
        "rpc",
        RelayOnly,
        Comment{
            "lokimq control address for for communicating with lokid. Depends on lokid's",
            "lmq-local-control configuration option. By default this value should be",
            "ipc://LOKID-DATA-DIRECTORY/lokid.sock, such as:",
            "    rpc=ipc:///var/lib/loki/lokid.sock",
            "    rpc=ipc:///home/USER/.loki/lokid.sock",
            "but can use (non-default) TCP if lokid is configured that way:",
            "    rpc=tcp://127.0.0.1:5678",
        },
        [this](std::string arg) { lokidRPCAddr = oxenmq::address(arg); });

    // Deprecated options:
    conf.defineOption<std::string>("lokid", "username", Deprecated);
    conf.defineOption<std::string>("lokid", "password", Deprecated);
    conf.defineOption<std::string>("lokid", "service-node-seed", Deprecated);
  }

  void
  BootstrapConfig::defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params)
  {
    (void)params;

    conf.defineOption<bool>(
        "bootstrap",
        "seed-node",
        Default{false},
        Comment{"Whether or not to run as a seed node. We will not have any bootstrap routers "
                "configured."},
        AssignmentAcceptor(seednode));

    conf.defineOption<std::string>(
        "bootstrap",
        "add-node",
        MultiValue,
        Comment{
            "Specify a bootstrap file containing a signed RouterContact of a service node",
            "which can act as a bootstrap. Can be specified multiple times.",
        },
        [this](std::string arg) {
          if (arg.empty())
          {
            throw std::invalid_argument("cannot use empty filename as bootstrap");
          }
          files.emplace_back(std::move(arg));
          if (not fs::exists(files.back()))
          {
            throw std::invalid_argument("file does not exist: " + arg);
          }
        });
  }

  void
  LoggingConfig::defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params)
  {
    (void)params;

    constexpr Default DefaultLogType{
        platform::is_android or platform::is_apple ? "system" : "print"};
    constexpr Default DefaultLogFile{""};
    constexpr Default DefaultLogLevel{"warn"};

    conf.defineOption<std::string>(
        "logging",
        "type",
        DefaultLogType,
        [this](std::string arg) { m_logType = log::type_from_string(arg); },
        Comment{
            "Log type (format). Valid options are:",
            "  print - print logs to standard output",
            "  system - logs directed to the system logger (syslog/eventlog/etc.)",
            "  file - plaintext formatting to a file",
        });

    conf.defineOption<std::string>(
        "logging",
        "level",
        DefaultLogLevel,
        [this](std::string arg) { m_logLevel = log::level_from_string(arg); },
        Comment{
            "Minimum log level to print. Logging below this level will be ignored.",
            "Valid log levels, in ascending order, are:",
            "  trace",
            "  debug",
            "  info",
            "  warn",
            "  error",
            "  critical",
            "  none",
        });

    conf.defineOption<std::string>(
        "logging",
        "file",
        DefaultLogFile,
        AssignmentAcceptor(m_logFile),
        Comment{
            "When using type=file this is the output filename.",
        });
  }

  void
  PeerSelectionConfig::defineConfigOptions(
      ConfigDefinition& conf, const ConfigGenParameters& params)
  {
    (void)params;

    constexpr Default DefaultUniqueCIDR{32};
    conf.defineOption<int>(
        "paths",
        "unique-range-size",
        DefaultUniqueCIDR,
        ClientOnly,
        [=](int arg) {
          if (arg == 0)
          {
            m_UniqueHopsNetmaskSize = arg;
          }
          else if (arg > 32 or arg < 4)
          {
            throw std::invalid_argument{"[paths]:unique-range-size must be between 4 and 32"};
          }
          m_UniqueHopsNetmaskSize = arg;
        },
        Comment{
            "Netmask for router path selection; each router must be from a distinct IP subnet "
            "of the given size.",
            "E.g. 16 ensures that all routers are using distinct /16 IP addresses."});

#ifdef WITH_GEOIP
    conf.defineOption<std::string>(
        "paths",
        "exclude-country",
        ClientOnly,
        MultiValue,
        [=](std::string arg) {
          m_ExcludeCountries.emplace(lowercase_ascii_string(std::move(arg)));
        },
        Comment{
            "exclude a country given its 2 letter country code from being used in path builds",
            "e.g. exclude-country=DE",
            "can be listed multiple times to exclude multiple countries"});
#endif
  }

  bool
  PeerSelectionConfig::Acceptable(const std::set<RouterContact>& rcs) const
  {
    if (m_UniqueHopsNetmaskSize == 0)
      return true;
    const auto netmask = netmask_ipv6_bits(96 + m_UniqueHopsNetmaskSize);
    std::set<IPRange> seenRanges;
    for (const auto& hop : rcs)
    {
      for (const auto& addr : hop.addrs)
      {
        const auto network_addr = net::In6ToHUInt(addr.ip) & netmask;
        if (auto [it, inserted] = seenRanges.emplace(network_addr, netmask); not inserted)
        {
          return false;
        }
      }
    }
    return true;
  }

  Config::Config(fs::path datadir)
      : m_DataDir(datadir.empty() ? fs::current_path() : std::move(datadir))
  {}

  constexpr auto GetOverridesDir = [](auto datadir) -> fs::path { return datadir / "conf.d"; };

  void
  Config::Save()
  {
    const auto overridesDir = GetOverridesDir(m_DataDir);
    if (not fs::exists(overridesDir))
      fs::create_directory(overridesDir);
    m_Parser.Save();
  }

  void
  Config::Override(std::string section, std::string key, std::string value)
  {
    m_Parser.AddOverride(GetOverridesDir(m_DataDir) / "overrides.ini", section, key, value);
  }

  void
  Config::LoadOverrides()
  {
    const auto overridesDir = GetOverridesDir(m_DataDir);
    if (fs::exists(overridesDir))
    {
      util::IterDir(overridesDir, [&](const fs::path& overrideFile) {
        if (overrideFile.extension() == ".ini")
        {
          m_Parser.LoadFile(overrideFile);
        }
        return true;
      });
    }
  }

  void
  Config::AddDefault(std::string section, std::string key, std::string val)
  {
    m_Additional.emplace_back(std::array<std::string, 3>{section, key, val});
  }

  bool
  Config::Load(std::optional<fs::path> fname, bool isRelay)
  {
    if (not fname.has_value())
      return LoadDefault(isRelay);
    try
    {
      ConfigGenParameters params;
      params.isRelay = isRelay;
      params.defaultDataDir = m_DataDir;

      ConfigDefinition conf{isRelay};
      initializeConfig(conf, params);
      addBackwardsCompatibleConfigOptions(conf);
      m_Parser.Clear();
      if (!m_Parser.LoadFile(*fname))
      {
        return false;
      }
      LoadOverrides();

      m_Parser.IterAll([&](std::string_view section, const SectionValues_t& values) {
        for (const auto& pair : values)
        {
          conf.addConfigValue(section, pair.first, pair.second);
        }
      });

      conf.acceptAllOptions();

      return true;
    }
    catch (const std::exception& e)
    {
      LogError("Error trying to init and parse config from file: ", e.what());
      return false;
    }
  }

  bool
  Config::LoadDefault(bool isRelay)
  {
    try
    {
      ConfigGenParameters params;
      params.isRelay = isRelay;
      params.defaultDataDir = m_DataDir;
      ConfigDefinition conf{isRelay};
      initializeConfig(conf, params);

      m_Parser.Clear();
      LoadOverrides();

      /// load additional config options added
      for (const auto& [sect, key, val] : m_Additional)
      {
        conf.addConfigValue(sect, key, val);
      }

      m_Parser.IterAll([&](std::string_view section, const SectionValues_t& values) {
        for (const auto& pair : values)
        {
          conf.addConfigValue(section, pair.first, pair.second);
        }
      });

      conf.acceptAllOptions();

      return true;
    }
    catch (const std::exception& e)
    {
      LogError("Error trying to init default config: ", e.what());
      return false;
    }
  }

  void
  Config::initializeConfig(ConfigDefinition& conf, const ConfigGenParameters& params)
  {
    router.defineConfigOptions(conf, params);
    network.defineConfigOptions(conf, params);
    paths.defineConfigOptions(conf, params);
    connect.defineConfigOptions(conf, params);
    dns.defineConfigOptions(conf, params);
    links.defineConfigOptions(conf, params);
    api.defineConfigOptions(conf, params);
    lokid.defineConfigOptions(conf, params);
    bootstrap.defineConfigOptions(conf, params);
    logging.defineConfigOptions(conf, params);
  }

  void
  Config::addBackwardsCompatibleConfigOptions(ConfigDefinition& conf)
  {
    // These config sections don't exist anymore:

    conf.defineOption<std::string>("system", "user", Deprecated);
    conf.defineOption<std::string>("system", "group", Deprecated);
    conf.defineOption<std::string>("system", "pidfile", Deprecated);

    conf.defineOption<std::string>("netdb", "dir", Deprecated);

    conf.defineOption<std::string>("metrics", "json-metrics-path", Deprecated);
  }

  void
  ensureConfig(fs::path dataDir, fs::path confFile, bool overwrite, bool asRouter)
  {
    // fail to overwrite if not instructed to do so
    if (fs::exists(confFile) && !overwrite)
    {
      LogDebug("Not creating config file; it already exists.");
      return;
    }

    const auto parent = confFile.parent_path();

    // create parent dir if it doesn't exist
    if ((not parent.empty()) and (not fs::exists(parent)))
    {
      fs::create_directory(parent);
    }

    llarp::LogInfo(
        "Attempting to create config file for ",
        (asRouter ? "router" : "client"),
        " at ",
        confFile);

    llarp::Config config{dataDir};
    std::string confStr;
    if (asRouter)
      confStr = config.generateBaseRouterConfig();
    else
      confStr = config.generateBaseClientConfig();

    // open a filestream
    auto stream = llarp::util::OpenFileStream<std::ofstream>(confFile.c_str(), std::ios::binary);
    if (not stream or not stream->is_open())
      throw std::runtime_error{fmt::format("Failed to open file {} for writing", confFile)};

    *stream << confStr;
    stream->flush();

    llarp::LogInfo("Generated new config ", confFile);
  }

  void
  generateCommonConfigComments(ConfigDefinition& def)
  {
    // router
    def.addSectionComments(
        "router",
        {
            "Configuration for routing activity.",
        });

    // logging
    def.addSectionComments(
        "logging",
        {
            "logging settings",
        });

    // api
    def.addSectionComments(
        "api",
        {
            "JSON API settings",
        });

    // dns
    def.addSectionComments(
        "dns",
        {
            "DNS configuration",
        });

    // bootstrap
    def.addSectionComments(
        "bootstrap",
        {
            "Configure nodes that will bootstrap us onto the network",
        });

    // network
    def.addSectionComments(
        "network",
        {
            "Network settings",
        });
  }

  std::string
  Config::generateBaseClientConfig()
  {
    ConfigGenParameters params;
    params.isRelay = false;
    params.defaultDataDir = m_DataDir;

    llarp::ConfigDefinition def{false};
    initializeConfig(def, params);
    generateCommonConfigComments(def);
    def.addSectionComments(
        "paths",
        {
            "path selection algorithm options",
        });

    def.addSectionComments(
        "network",
        {
            "Snapp settings",
        });

    return def.generateINIConfig(true);
  }

  std::string
  Config::generateBaseRouterConfig()
  {
    ConfigGenParameters params;
    params.isRelay = true;
    params.defaultDataDir = m_DataDir;

    llarp::ConfigDefinition def{true};
    initializeConfig(def, params);
    generateCommonConfigComments(def);

    // lokid
    def.addSectionComments(
        "lokid",
        {
            "Settings for communicating with lokid",
        });

    return def.generateINIConfig(true);
  }

  std::shared_ptr<Config>
  Config::EmbeddedConfig()
  {
    auto config = std::make_shared<Config>(fs::path{});
    config->Load();
    config->logging.m_logLevel = log::Level::off;
    config->api.m_enableRPCServer = false;
    config->network.m_endpointType = "null";
    config->network.m_saveProfiles = false;
    config->bootstrap.files.clear();
    return config;
  }

}  // namespace llarp
