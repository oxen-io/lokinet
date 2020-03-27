#include <config/config.hpp>

#include <config/ini.hpp>
#include <constants/defaults.hpp>
#include <net/net.hpp>
#include <router_contact.hpp>
#include <stdexcept>
#include <util/fs.hpp>
#include <util/logging/logger_syslog.hpp>
#include <util/logging/logger.hpp>
#include <util/mem.hpp>
#include <util/str.hpp>
#include <util/lokinet_init.h>

#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include "ghc/filesystem.hpp"

namespace llarp
{

  // constants for config file default values
  constexpr int DefaultMinConnectionsForRouter = 6;
  constexpr int DefaultMaxConnectionsForRouter = 60;

  constexpr int DefaultMinConnectionsForClient = 4;
  constexpr int DefaultMaxConnectionsForClient = 6;

  LoggingConfig::LogType
  LoggingConfig::LogTypeFromString(const std::string& str)
  {
    if (str == "unknown") return LogType::Unknown;
    else if (str == "file") return LogType::File;
    else if (str == "json") return LogType::Json;
    else if (str == "syslog") return LogType::Syslog;

    return LogType::Unknown;
  }

  void
  RouterConfig::defineConfigOptions(Configuration& conf, const ConfigGenParameters& params)
  {
    conf.defineOption<int>("router", "job-queue-size", false, m_JobQueueSize,
      [this](int arg) {
        if (arg < 1024)
          throw std::invalid_argument("job-queue-size must be 1024 or greater");

        m_JobQueueSize = arg;
      });

    // TODO: we don't support other protocols now; remove
    conf.defineOption<std::string>("router", "default-protocol", false, m_DefaultLinkProto,
    [this](std::string arg) {
        m_DefaultLinkProto = arg;
      });

    conf.defineOption<std::string>("router", "netid", true, m_netId,
      [this](std::string arg) {
        if(arg.size() > NetID::size())
          throw std::invalid_argument(stringify(
                "netid is too long, max length is ", NetID::size()));

        m_netId = std::move(arg);
      });

    int minConnections = (params.isRelay ? DefaultMinConnectionsForRouter
                                   : DefaultMinConnectionsForClient);
    conf.defineOption<int>("router", "min-connections", false, minConnections,
      [=](int arg) {
        if (arg < minConnections)
          throw std::invalid_argument(stringify("min-connections must be >= ", minConnections));

        m_minConnectedRouters = arg;
      });

    int maxConnections = (params.isRelay ? DefaultMaxConnectionsForRouter
                                   : DefaultMaxConnectionsForClient);
    conf.defineOption<int>("router", "max-connections", false, maxConnections,
      [=](int arg) {
        if (arg < maxConnections)
          throw std::invalid_argument(stringify("max-connections must be >= ", maxConnections));

        m_maxConnectedRouters = arg;
      });

    // additional check that min <= max
    // TODO: where can we perform this check now?
    // if (m_minConnectedRouters > m_maxConnectedRouters)
      // throw std::invalid_argument("[router]:min-connections must be less than [router]:max-connections");

    conf.defineOption<std::string>("router", "nickname", false, m_nickname,
      [this](std::string arg) {
        m_nickname = std::move(arg);
      });

    conf.defineOption<std::string>("router", "data-dir", false, GetDefaultDataDir(),
      [this](std::string arg) {
        m_dataDir = std::move(arg);
      });

    conf.defineOption<std::string>("router", "public-address", false, "",
      [this](std::string arg) {
        llarp::LogInfo("public ip ", arg, " size ", arg.size());
        if(arg.size() < 17)
        {
          // assume IPv4
          llarp::Addr a(arg);
          llarp::LogInfo("setting public ipv4 ", a);
          m_addrInfo.ip    = *a.addr6();
          m_publicOverride = true;
        }
      });

    conf.defineOption<int>("router", "public-port", false, 1090,
      [this](int arg) {
        if (arg <= 0)
          throw std::invalid_argument("public-port must be > 0");

        // Not needed to flip upside-down - this is done in llarp::Addr(const AddressInfo&)
        m_ip4addr.sin_port = arg;
        m_addrInfo.port    = arg;
        m_publicOverride   = true;
      });

    conf.defineOption<int>("router", "worker-threads", false, m_workerThreads,
      [this](int arg) {
        if (arg <= 0)
          throw std::invalid_argument("worker-threads must be > 0");

        m_workerThreads = arg;
      });

    conf.defineOption<int>("router", "net-threads", false, m_numNetThreads, 
      [this](int arg) {
        if (arg <= 0)
          throw std::invalid_argument("net-threads must be > 0");

        m_numNetThreads = arg;
      });

    conf.defineOption<bool>("router", "block-bogons", false, m_blockBogons,
      [this](bool arg) {
        m_blockBogons = arg;
      });
  }

  void
  NetworkConfig::defineConfigOptions(Configuration& conf, const ConfigGenParameters& params)
  {
    (void)params;

    // TODO: review default value
    conf.defineOption<bool>("network", "profiling", false, m_enableProfiling,
      [this](bool arg) {
        m_enableProfiling = arg;
      });

    conf.defineOption<std::string>("network", "profiles", false, m_routerProfilesFile,
      [this](std::string arg) {
        m_routerProfilesFile = std::move(arg);
      });

    conf.defineOption<std::string>("network", "strict-connect", false, m_strictConnect,
      [this](std::string arg) {
        m_strictConnect = std::move(arg);
      });

    // TODO: NetConfig was collecting all other k:v pairs here
  }

  void
  NetdbConfig::defineConfigOptions(Configuration& conf, const ConfigGenParameters& params)
  {
    (void)params;

    conf.defineOption<std::string>("netdb", "dir", false, m_nodedbDir,
      [this](std::string arg) {
        m_nodedbDir = str(arg);
      });
  }

  void
  DnsConfig::defineConfigOptions(Configuration& conf, const ConfigGenParameters& params)
  {
    (void)params;

    // TODO: this was previously a multi-value option
    conf.defineOption<std::string>("dns", "upstream", false, "",
      [this](std::string arg) {
        netConfig.emplace("upstream-dns", std::move(arg));
      });

    // TODO: this was previously a multi-value option
    conf.defineOption<std::string>("dns", "bind", false, "",
      [this](std::string arg) {
        netConfig.emplace("local-dns", std::move(arg));
      });
  }

  LinksConfig::LinkInfo
  LinksConfig::LinkInfoFromINIValues(string_view name, string_view value)
  {
    // we treat the INI k:v pair as:
    // k: interface name, * indicating outbound
    // v: a comma-separated list of values, an int indicating port (everything else ignored)
    //    this is somewhat of a backwards- and forwards-compatibility thing

    LinkInfo info;
    info.addressFamily = AF_INET;
    info.interface = str(name);

    std::vector<string_view> splits = split(value, ',');
    for (string_view str : splits)
    {
      int asNum = std::atoi(str.data());
      if (asNum > 0)
        info.port = asNum;

      // otherwise, ignore ("future-proofing")
    }

    return info;
  }

  void
  LinksConfig::defineConfigOptions(Configuration& conf, const ConfigGenParameters& params)
  {
    (void)params;

    conf.addUndeclaredHandler("bind", [&](string_view, string_view name, string_view value) {
      LinkInfo info = LinkInfoFromINIValues(name, value);

      if (info.port <= 0)
        throw std::invalid_argument(stringify("Invalid [bind] port specified on interface", name));

      if(name == "*")
        m_OutboundLink = std::move(info);
      else
        m_InboundLinks.emplace_back(std::move(info));

      return true;
    });

  }

  void
  ConnectConfig::defineConfigOptions(Configuration& conf, const ConfigGenParameters& params)
  {
    (void)params;


    conf.addUndeclaredHandler("connect", [this](string_view section,
                                                string_view name,
                                                string_view value) {
      (void)section;
      (void)name;
      routers.emplace_back(value);
      return true;
    });
  }

  void
  ServicesConfig::defineConfigOptions(Configuration& conf, const ConfigGenParameters& params)
  {
    (void)params;

    conf.addUndeclaredHandler("services", [this](string_view section,
                                                 string_view name,
                                                 string_view value) {
      (void)section;
      services.emplace_back(name, value);
      return true;
    });
  }

  void
  SystemConfig::defineConfigOptions(Configuration& conf, const ConfigGenParameters& params)
  {
    (void)params;

    conf.defineOption<std::string>("system", "pidfile", false, pidfile,
      [this](std::string arg) {
        pidfile = std::move(arg);
      });
  }

  void
  ApiConfig::defineConfigOptions(Configuration& conf, const ConfigGenParameters& params)
  {
    (void)params;

    conf.defineOption<bool>("api", "enabled", false, m_enableRPCServer,
      [this](bool arg) {
        m_enableRPCServer = arg;
      });

    conf.defineOption<std::string>("api", "bind", false, m_rpcBindAddr,
      [this](std::string arg) {
        m_rpcBindAddr = std::move(arg);
      });
    // TODO: this was from pre-refactor:
      // TODO: add pubkey to whitelist
  }

  void
  LokidConfig::defineConfigOptions(Configuration& conf, const ConfigGenParameters& params)
  {
    (void)params;

    conf.defineOption<std::string>("lokid", "service-node-seed", false, "",
      [this](std::string arg) {
       if (not arg.empty())
       {
        usingSNSeed = true;
        ident_keyfile = std::move(arg);
       }
      });

    conf.defineOption<bool>("lokid", "enabled", false, whitelistRouters,
      [this](bool arg) {
        whitelistRouters = arg;
      });

    // TODO: was also aliased as "addr" -- presumably because loki-launcher
    conf.defineOption<std::string>("lokid", "jsonrpc", false, lokidRPCAddr,
      [this](std::string arg) {
        lokidRPCAddr = arg;
      });

    conf.defineOption<std::string>("lokid", "username", false, lokidRPCUser,
      [this](std::string arg) {
        lokidRPCUser = arg;
      });

    conf.defineOption<std::string>("lokid", "password", false, lokidRPCPassword,
      [this](std::string arg) {
        lokidRPCPassword = arg;
      });
  }

  void
  BootstrapConfig::defineConfigOptions(Configuration& conf, const ConfigGenParameters& params)
  {
    (void)params;

    conf.addUndeclaredHandler("bootstrap", [&](string_view, string_view name, string_view value) {
      if (name != "add-node")
      {
        return false;
      }
      else
      {
        routers.emplace_back(str(value));
        return true;
      }
    });
  }

  void
  LoggingConfig::defineConfigOptions(Configuration& conf, const ConfigGenParameters& params)
  {
    (void)params;

    conf.defineOption<std::string>("logging", "type", false, "file",
      [this](std::string arg) {
      LoggingConfig::LogType type = LogTypeFromString(arg);
        if (type == LogType::Unknown)
          throw std::invalid_argument(stringify("invalid log type: ", arg));

        m_logType = type;
      });

    conf.defineOption<std::string>("logging", "level", false, "info",
      [this](std::string arg) {
        nonstd::optional<LogLevel> level = LogLevelFromString(arg);
        if (not level.has_value())
          throw std::invalid_argument(stringify( "invalid log level value: ", arg));

        m_logLevel = level.value();
      });

    conf.defineOption<std::string>("logging", "file", false, "stdout",
      [this](std::string arg) {
        m_logFile = arg;
      });
  }

  bool
  Config::Load(const char *fname, bool isRelay, fs::path defaultDataDir)
  {
    try
    {
      ConfigGenParameters params;
      params.isRelay = isRelay;
      params.defaultDataDir = std::move(defaultDataDir);

      Configuration conf;
      initializeConfig(conf, params);

      ConfigParser parser;
      if(!parser.LoadFile(fname))
      {
        return false;
      }

      parser.IterAll([&](string_view section, const SectionValues_t& values) {
        for (const auto& pair : values)
        {
          conf.addConfigValue(section, pair.first, pair.second);
        }
      });

      // TODO: hand parsed data to conf, pull out values
      return true;
    }
    catch(const std::exception& e)
    {
      LogError("Error trying to init and parse config from file: ", e.what());
      return false;
    }
  }

  void
  Config::initializeConfig(Configuration& conf, const ConfigGenParameters& params)
  {
    // TODO: this seems like a random place to put this, should this be closer
    //       to main() ?
    if(Lokinet_INIT())
      throw std::runtime_error("Can't initializeConfig() when Lokinet_INIT() == true");

    router.defineConfigOptions(conf, params);
    network.defineConfigOptions(conf, params);
    connect.defineConfigOptions(conf, params);
    netdb.defineConfigOptions(conf, params);
    dns.defineConfigOptions(conf, params);
    links.defineConfigOptions(conf, params);
    services.defineConfigOptions(conf, params);
    system.defineConfigOptions(conf, params);
    api.defineConfigOptions(conf, params);
    lokid.defineConfigOptions(conf, params);
    bootstrap.defineConfigOptions(conf, params);
    logging.defineConfigOptions(conf, params);
  }

  fs::path
  GetDefaultDataDir()
  {
#ifdef _WIN32
    const fs::path homedir = fs::path(getenv("APPDATA"));
#else
    const fs::path homedir = fs::path(getenv("HOME"));
#endif
    return homedir / fs::path(".lokinet/");
  }

  fs::path
  GetDefaultConfigFilename()
  {
    return fs::path("lokinet.ini");
  }

  fs::path
  GetDefaultConfigPath()
  {
    return GetDefaultDataDir() / GetDefaultConfigFilename();
  }

  void
  ensureConfig(const fs::path& defaultDataDir,
               const fs::path& confFile,
               bool overwrite,
               bool asRouter)
  {
    std::error_code ec;

    // fail to overwrite if not instructed to do so
    if(fs::exists(confFile, ec) && !overwrite)
      throw std::invalid_argument(stringify("Config file ", confFile, " already exists"));

    if (ec) throw std::runtime_error(stringify("filesystem error: ", ec));

    // create parent dir if it doesn't exist
    if (not fs::exists(confFile.parent_path(), ec))
    {
      if (not fs::create_directory(confFile.parent_path()))
        throw std::runtime_error(stringify("Failed to create parent directory for ", confFile));
    }
    if (ec) throw std::runtime_error(stringify("filesystem error: ", ec));

    llarp::LogInfo("Attempting to create config file, asRouter: ", asRouter,
                   " path: ", confFile);

    llarp::Config config;
    std::string confStr;
    if (asRouter)
      confStr = config.generateBaseRouterConfig(std::move(defaultDataDir));
    else
      confStr = config.generateBaseClientConfig(std::move(defaultDataDir));

    // open a filestream
    auto stream = llarp::util::OpenFileStream<std::ofstream>(confFile.c_str(), std::ios::binary);
    if (not stream.has_value() or not stream.value().is_open())
      throw std::runtime_error(stringify("Failed to open file ", confFile, " for writing"));

    llarp::LogInfo("confStr: ", confStr);

    stream.value() << confStr;
    stream.value().flush();

    llarp::LogInfo("Generated new config ", confFile);
  }

  std::string
  Config::generateBaseClientConfig(fs::path defaultDataDir)
  {
    ConfigGenParameters params;
    params.isRelay = false;
    params.defaultDataDir = std::move(defaultDataDir);

    llarp::Configuration def;
    initializeConfig(def, params);

    // router
    def.addSectionComment("router", "Configuration for routing activity.");

    def.addOptionComment("router", "threads",
        "The number of threads available for performing cryptographic functions.");
    def.addOptionComment("router", "threads",
        "The minimum is one thread, but network performance may increase with more.");
    def.addOptionComment("router", "threads",
        "threads. Should not exceed the number of logical CPU cores.");

    def.addOptionComment("router", "data-dir",
        "Optional directory for containing lokinet runtime data. This includes generated");
    def.addOptionComment("router", "data-dir",
        "private keys.");

    // TODO: why did Kee want this, and/or what does it really do? Something about logs?
    def.addOptionComment("router", "nickname", "Router nickname. Kee wanted it.");

    def.addOptionComment("router", "min-connections",
        "Minimum number of routers lokinet will attempt to maintain connections to.");

    def.addOptionComment("router", "max-connections",
        "Maximum number (hard limit) of routers lokinet will be connected to at any time.");

    // logging
    def.addSectionComment("logging", "logging settings");

    def.addOptionComment("logging", "level",
        "Minimum log level to print. Logging below this level will be ignored.");
    def.addOptionComment("logging", "level", "Valid log levels, in ascending order, are:");
    def.addOptionComment("logging", "level", "  trace");
    def.addOptionComment("logging", "level", "  debug");
    def.addOptionComment("logging", "level", "  info");
    def.addOptionComment("logging", "level", "  warn");
    def.addOptionComment("logging", "level", "  error");

    def.addOptionComment("logging", "type", "Log type (format). Valid options are:");
    def.addOptionComment("logging", "type", "  file - plaintext formatting");
    def.addOptionComment("logging", "type", "  json - json-formatted log statements");
    def.addOptionComment("logging", "type", "  syslog - logs directed to syslog");

    // api
    def.addSectionComment("api", "JSON API settings");

    def.addOptionComment("api", "enabled", "Determines whether or not the JSON API is enabled.");

    def.addOptionComment("api", "bind", "IP address and port to bind to.");
    def.addOptionComment("api", "bind", "Recommend localhost-only for security purposes.");

    // system
    def.addSectionComment("system", "System setings for running lokinet.");

    def.addOptionComment("system", "user", "The user which lokinet should run as.");

    def.addOptionComment("system", "group", "The group which lokinet should run as.");

    def.addOptionComment("system", "pidfile", "Location of the pidfile for lokinet.");

    // dns
    def.addSectionComment("dns", "DNS configuration");

    def.addOptionComment("dns", "upstream",
        "Upstream resolver to use as fallback for non-loki addresses.");
    def.addOptionComment("dns", "upstream", "Multiple values accepted.");

    def.addOptionComment("dns", "bind", "Address to bind to for handling DNS requests.");
    def.addOptionComment("dns", "bind", "Multiple values accepted.");

    // netdb
    def.addSectionComment("netdb", "Configuration for lokinet's database of service nodes");

    def.addOptionComment("netdb", "dir", "Root directory of netdb.");

    // bootstrap
    def.addSectionComment("bootstrap", "Configure nodes that will bootstrap us onto the network");

    def.addOptionComment("bootstrap", "add-node",
        "Specify a bootstrap file containing a signed RouterContact of a service node");
    def.addOptionComment("bootstrap", "add-node",
        "which can act as a bootstrap. Accepts multiple values.");

    // network
    def.addSectionComment("network", "Network settings");

    def.addOptionComment("network", "profiles", "File to contain router profiles.");

    def.addOptionComment("network", "strict-connect",
        "Public key of a router which will act as sole first-hop. This may be used to");
    def.addOptionComment("network", "strict-connect",
        "provide a trusted router (consider that you are not fully anonymous with your");
    def.addOptionComment("network", "strict-connect",
        "first hop).");

    def.addOptionComment("network", "exit-node", "Public key of an exit-node.");

    def.addOptionComment("network", "ifname", "Interface name for lokinet traffic.");

    def.addOptionComment("network", "ifaddr", "Local IP address for lokinet traffic.");

    return def.generateINIConfig(true);
  }

  std::string
  Config::generateBaseRouterConfig(fs::path defaultDataDir)
  {
    ConfigGenParameters params;
    params.isRelay = true;
    params.defaultDataDir = std::move(defaultDataDir);

    llarp::Configuration def;
    initializeConfig(def, params);

    // lokid
    def.addSectionComment("lokid", "Lokid configuration (settings for talking to lokid");

    def.addOptionComment("lokid", "enabled",
        "Whether or not we should talk to lokid. Must be enabled for staked routers.");

    def.addOptionComment("lokid", "jsonrpc",
        "Host and port of running lokid that we should talk to.");

    // TODO: doesn't appear to be used in the codebase
    def.addOptionComment("lokid", "service-node-seed", "File containing service node's seed.");

    // extra [network] options
    // TODO: probably better to create an [exit] section and only allow it for routers
    def.addOptionComment("network", "exit",
        "Whether or not we should act as an exit node. Beware that this increases demand");
    def.addOptionComment("network", "exit",
        "on the server and may pose liability concerns. Enable at your own risk.");

    // TODO: define the order of precedence (e.g. is whitelist applied before blacklist?)
    //       additionally, what's default? What if I don't whitelist anything?
    def.addOptionComment("network", "exit-whitelist",
        "List of destination protocol:port pairs to whitelist, example: udp:*");
    def.addOptionComment("network", "exit-whitelist",
        "or tcp:80. Multiple values supported.");

    def.addOptionComment("network", "exit-blacklist",
        "Blacklist of destinations (same format as whitelist).");

    return def.generateINIConfig(true);
  }

}  // namespace llarp

bool
llarp_ensure_client_config(std::ofstream& f, std::string basepath)
{

  return true;

  /*
   * TODO: remove this function. comments left as evidence of what a snapp config does
   *
   *
  // write snapp-example.ini
  const std::string snappExample_fpath = basepath + "snapp-example.ini";
  {
    auto stream = llarp::util::OpenFileStream<std::ofstream>(snappExample_fpath, std::ios::binary);
    if (!stream)
    {
      return false;
    }
    auto& example_f = stream.value();
    if (example_f.is_open())
    {
      // pick ip
      // don't revert me
      const static std::string ip = "10.33.0.1/16";

      // std::string ip = llarp::findFreePrivateRange();
      // if(ip == "")
      // {
      //   llarp::LogError(
      //       "Couldn't easily detect a private range to map lokinet onto");
      //   return false;
      // }
      example_f << "# this is an example configuration for a snapp\n";
      example_f << "[example-snapp]\n";
      example_f << "# keyfile is the path to the private key of the snapp, "
                   "your .loki is tied to this key, DON'T LOSE IT\n";
      example_f << "keyfile=" << basepath << "example-snap-keyfile.private\n";
      example_f << "# ifaddr is the ip range to allocate to this snapp\n";
      example_f << "ifaddr=" << ip << std::endl;
      // probably fine to leave this (and not-auto-detect it) I'm not worried
      // about any collisions
      example_f << "# ifname is the name to try and give to the network "
                   "interface this snap owns\n";
      example_f << "ifname=snapp-tun0\n";
    }
    else
    {
      llarp::LogError("failed to write ", snappExample_fpath);
    }
  }
  // now do up fname
  f << "\n\n";
  f << "# snapps configuration section\n";
  f << "[services]\n";
  f << "# uncomment next line to enable a snapp\n";
  f << "#example-snapp=" << snappExample_fpath << std::endl;
  f << "\n\n";
  return true;
  */
}
