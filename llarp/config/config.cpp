#include <config/config.hpp>

#include <config/ini.hpp>
#include <constants/defaults.hpp>
#include <constants/limits.hpp>
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
  const char*
  lokinetEnv(string_view suffix)
  {
    std::string env;
    env.reserve(8 + suffix.size());
    env.append("LOKINET_"s);
    env.append(suffix.begin(), suffix.end());
    return std::getenv(env.c_str());
  }

  std::string
  fromEnv(string_view val, string_view envNameSuffix)
  {
    if (const char* ptr = lokinetEnv(envNameSuffix))
      return ptr;
    return {val.begin(), val.end()};
  }

  int
  fromEnv(const int& val, string_view envNameSuffix)
  {
    if (const char* ptr = lokinetEnv(envNameSuffix))
      return std::atoi(ptr);
    return val;
  }

  uint16_t
  fromEnv(const uint16_t& val, string_view envNameSuffix)
  {
    if (const char* ptr = lokinetEnv(envNameSuffix))
      return std::atoi(ptr);

    return val;
  }

  size_t
  fromEnv(const size_t& val, string_view envNameSuffix)
  {
    if (const char* ptr = lokinetEnv(envNameSuffix))
      return std::atoll(ptr);

    return val;
  }

  nonstd::optional<bool>
  fromEnv(const nonstd::optional<bool>& val, string_view envNameSuffix)
  {
    if (const char* ptr = lokinetEnv(envNameSuffix))
      return IsTrueValue(ptr);

    return val;
  }

  LoggingConfig::LogType LoggingConfig::LogTypeFromString(const std::string& str)
  {
    if (str == "unknown") return LogType::Unknown;
    else if (str == "file") return LogType::File;
    else if (str == "json") return LogType::Json;
    else if (str == "syslog") return LogType::Syslog;

    return LogType::Unknown;
  }

  void
  RouterConfig::defineConfigOptions(Configuration& conf)
  {
    conf.defineOption<int>("router", "job-queue-size", false, m_JobQueueSize,
      [this](int arg) {
        if (arg < 1024)
          throw std::invalid_argument("job-queue-size must be 1024 or greater");

        m_JobQueueSize = arg;
      });

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

    conf.defineOption<int>("router", "max-connections", false, m_maxConnectedRouters,
      [this](int arg) {
        if (arg < 1)
          throw std::invalid_argument("max-connections must be >= 1");

        m_maxConnectedRouters = arg;
      });

    conf.defineOption<int>("router", "min-connections", false, m_minConnectedRouters,
      [this](int arg) {
        if (arg < 1)
          throw std::invalid_argument("min-connections must be >= 1");

        m_minConnectedRouters = arg;
      });

    // additional check that min <= max
    // TODO: where can we perform this check now?
    // if (m_minConnectedRouters > m_maxConnectedRouters)
      // throw std::invalid_argument("[router]:min-connections must be less than [router]:max-connections");

    conf.defineOption<std::string>("router", "nickname", false, m_nickname,
      [this](std::string arg) {
        m_nickname = std::move(arg);
        // TODO: side effect here, no side effects in config parsing!!
        LogContext::Instance().nodeName = nickname();
      });

    conf.defineOption<std::string>("router", "encryption-privkey", false, m_encryptionKeyfile,
      [this](std::string arg) {
        m_encryptionKeyfile = std::move(arg);
      });

    conf.defineOption<std::string>("router", "contact-file", false, m_ourRcFile,
      [this](std::string arg) {
        m_ourRcFile = std::move(arg);
      });

    conf.defineOption<std::string>("router", "transport-privkey", false, m_transportKeyfile,
      [this](std::string arg) {
        m_transportKeyfile = std::move(arg);
      });

    conf.defineOption<std::string>("router", "identity-privkey", false, m_identKeyfile,
      [this](std::string arg) {
        m_identKeyfile = std::move(arg);
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
  NetworkConfig::defineConfigOptions(Configuration& conf)
  {
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
  NetdbConfig::defineConfigOptions(Configuration& conf)
  {
    conf.defineOption<std::string>("netdb", "dir", false, m_nodedbDir,
      [this](std::string arg) {
        m_nodedbDir = str(arg);
      });
  }

  void
  DnsConfig::defineConfigOptions(Configuration& conf)
  {
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
  LinksConfig::defineConfigOptions(Configuration& conf)
  {
    conf.addUndeclaredHandler("bind", [&](string_view, string_view name, string_view value) {
      LinkInfo info = LinkInfoFromINIValues(name, value);

      if (info.port <= 0)
        throw std::invalid_argument(stringify("Invalid [bind] port specified on interface", name));

      if(name == "*")
      {
        info.port = fromEnv(info.port, "OUTBOUND_PORT");
        m_OutboundLink = std::move(info);
      }
      else
      {
        m_InboundLinks.emplace_back(std::move(info));
      }

      return true;
    });

  }

  void
  ConnectConfig::defineConfigOptions(Configuration& conf)
  {

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
  ServicesConfig::defineConfigOptions(Configuration& conf)
  {
    conf.addUndeclaredHandler("services", [this](string_view section,
                                                 string_view name,
                                                 string_view value) {
      (void)section;
      services.emplace_back(name, value);
      return true;
    });
  }

  void
  SystemConfig::defineConfigOptions(Configuration& conf)
  {
    conf.defineOption<std::string>("system", "pidfile", false, pidfile,
      [this](std::string arg) {
        pidfile = std::move(arg);
      });
  }

  void
  ApiConfig::defineConfigOptions(Configuration& conf)
  {
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
  LokidConfig::defineConfigOptions(Configuration& conf)
  {
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
  BootstrapConfig::defineConfigOptions(Configuration& conf)
  {
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
  LoggingConfig::defineConfigOptions(Configuration& conf)
  {
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
  Config::Load(const char *fname)
  {
    // TODO: DRY
    try
    {
      Configuration conf;
      initializeConfig(conf);

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

  bool
  Config::LoadFromStr(string_view str)
  {
    // TODO: DRY
    try
    {
      Configuration conf;
      initializeConfig(conf);

      ConfigParser parser;
      if(!parser.LoadFromStr(str))
      {
        return false;
      }

      // TODO: hand parsed data to conf, pull out values
      return true;
    }
    catch(const std::exception& e)
    {
      LogError("Error trying to init and parse config from string: ", e.what());
      return false;
    }
  }

  void
  Config::initializeConfig(Configuration& conf)
  {
    // TODO: this seems like a random place to put this, should this be closer
    //       to main() ?
    if(Lokinet_INIT())
      throw std::runtime_error("Can't initializeConfig() when Lokinet_INIT() == true");

    router.defineConfigOptions(conf);
    network.defineConfigOptions(conf);
    connect.defineConfigOptions(conf);
    netdb.defineConfigOptions(conf);
    dns.defineConfigOptions(conf);
    links.defineConfigOptions(conf);
    services.defineConfigOptions(conf);
    system.defineConfigOptions(conf);
    api.defineConfigOptions(conf);
    lokid.defineConfigOptions(conf);
    bootstrap.defineConfigOptions(conf);
    logging.defineConfigOptions(conf);
  }

  fs::path
  GetDefaultConfigDir()
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
    return GetDefaultConfigDir() / GetDefaultConfigFilename();
  }

  void
  ensureConfig(const fs::path& dir, const fs::path& filename, bool overwrite, bool asRouter)
  {
    fs::path fullPath = dir / filename;

    std::error_code ec;

    // fail to overwrite if not instructed to do so
    if(fs::exists(fullPath, ec) && !overwrite)
      throw std::invalid_argument(stringify("Config file ", fullPath, " already exists"));

    if (ec) throw std::runtime_error(stringify("filesystem error: ", ec));

    // create parent dir if it doesn't exist
    if (not fs::exists(dir, ec))
    {
      if (not fs::create_directory(dir))
        throw std::runtime_error(stringify("Failed to create parent directory ", dir));
    }
    if (ec) throw std::runtime_error(stringify("filesystem error: ", ec));

    llarp::LogInfo("Attempting to create config file ", fullPath);

    llarp::Config config;
    std::string confStr;
    if (asRouter)
      confStr = config.generateBaseClientConfig();
    else
      confStr = config.generateBaseRouterConfig();

    // open a filestream
    auto stream = llarp::util::OpenFileStream<std::ofstream>(fullPath.c_str(), std::ios::binary);
    if (not stream.has_value() or not stream.value().is_open())
      throw std::runtime_error(stringify("Failed to open file ", fullPath, " for writing"));

    stream.value() << confStr;
    stream.value().flush();

    llarp::LogInfo("Generated new config ", fullPath);
  }

  std::string
  Config::generateBaseClientConfig()
  {
    throw std::runtime_error("fixme");
  }

  std::string
  Config::generateBaseRouterConfig()
  {
    throw std::runtime_error("fixme");
  }

}  // namespace llarp

void
llarp_generic_ensure_config(std::ofstream& f, std::string basepath, bool isRouter)
{
  llarp::Configuration def;
  llarp::Config conf;
  conf.initializeConfig(def);

  std::string confStr = def.generateINIConfig();
  f << confStr;
  f.flush();


  /*
  f << "# this configuration was auto generated with 'sane' defaults\n";
  f << "# change these values as desired\n";
  f << "\n\n";
  f << "[router]\n";
  f << "# number of crypto worker threads \n";
  f << "threads=4\n";
  f << "# path to store signed RC\n";
  f << "contact-file=" << basepath << "self.signed\n";
  f << "# path to store transport private key\n";
  f << "transport-privkey=" << basepath << "transport.private\n";
  f << "# path to store identity signing key\n";
  f << "ident-privkey=" << basepath << "identity.private\n";
  f << "# encryption key for onion routing\n";
  f << "encryption-privkey=" << basepath << "encryption.private\n";
  f << std::endl;
  f << "# uncomment following line to set router nickname to 'lokinet'" << std::endl;
  f << "#nickname=lokinet\n";
  const auto limits = isRouter ? llarp::limits::snode : llarp::limits::client;

  f << "# maintain min connections to other routers\n";
  f << "min-routers=" << std::to_string(limits.DefaultMinRouters) << std::endl;
  f << "# hard limit of routers globally we are connected to at any given "
       "time\n";
  f << "max-routers=" << std::to_string(limits.DefaultMaxRouters) << std::endl;
  f << "\n\n";

  // logging
  f << "[logging]\n";
  f << "level=info\n";
  f << "# uncomment for logging to file\n";
  f << "#type=file\n";
  f << "#file=/path/to/logfile\n";
  f << "# uncomment for syslog logging\n";
  f << "#type=syslog\n";

  f << "\n\n";

  f << "# admin api\n";
  f << "[api]\n";
  f << "enabled=true\n";
  f << "#authkey=insertpubkey1here\n";
  f << "#authkey=insertpubkey2here\n";
  f << "#authkey=insertpubkey3here\n";
  f << "bind=127.0.0.1:1190\n";
  f << "\n\n";

  f << "# system settings for privileges and such\n";
  f << "[system]\n";
  f << "user=" << DEFAULT_LOKINET_USER << std::endl;
  f << "group=" << DEFAULT_LOKINET_GROUP << std::endl;
  f << "pidfile=" << basepath << "lokinet.pid\n";
  f << "\n\n";

  f << "# dns provider configuration section\n";
  f << "[dns]\n";
  f << "# resolver\n";
  f << "upstream=" << DEFAULT_RESOLVER_US << std::endl;

// Make auto-config smarter
// will this break reproducibility rules?
// (probably)
#ifdef __linux__
#ifdef ANDROID
  f << "bind=127.0.0.1:1153\n";
#else
  f << "bind=127.3.2.1:53\n";
#endif
#else
  f << "bind=127.0.0.1:53\n";
#endif
  f << "\n\n";

  f << "# network database settings block \n";
  f << "[netdb]\n";
  f << "# directory for network database skiplist storage\n";
  f << "dir=" << basepath << "netdb\n";
  f << "\n\n";

  f << "# bootstrap settings\n";
  f << "[bootstrap]\n";
  f << "# add a bootstrap node's signed identity to the list of nodes we want "
       "to bootstrap from\n";
  f << "# if we don't have any peers we connect to this router\n";
  f << "add-node=" << basepath << "bootstrap.signed\n";
  // we only process one of these...
  // f << "# add another bootstrap node\n";
  // f << "#add-node=/path/to/alternative/self.signed\n";
  f << "\n\n";
  */
}

void
llarp_ensure_router_config(std::ofstream& f, std::string basepath)
{
  llarp::Configuration def;
  llarp::Config conf;
  conf.initializeConfig(def);

  std::string confStr = def.generateINIConfig();

  /*
  f << confStr;
  f.flush();
  f << "# lokid settings (disabled by default)\n";
  f << "[lokid]\n";
  f << "enabled=false\n";
  f << "jsonrpc=127.0.0.1:22023\n";
  f << "#service-node-seed=/path/to/servicenode/seed\n";
  f << std::endl;
  f << "# network settings \n";
  f << "[network]\n";
  f << "profiles=" << basepath << "profiles.dat\n";
  // better to let the routers auto-configure
  // f << "ifaddr=auto\n";
  // f << "ifname=auto\n";
  f << "enabled=true\n";
  f << "exit=false\n";
  f << "#exit-blacklist=tcp:25\n";
  f << "#exit-whitelist=tcp:*\n";
  f << "#exit-whitelist=udp:*\n";
  f << std::endl;
  f << "# ROUTERS ONLY: publish network interfaces for handling inbound "
       "traffic\n";
  f << "[bind]\n";
  // get ifname
  std::string ifname;
  if (llarp::GetBestNetIF(ifname, AF_INET))
  {
    f << ifname << "=1090\n";
  }
  else
  {
    f << "# could not autodetect network interface\n"
      << "#eth0=1090\n";
  }

  f << std::endl;
  */
}

bool
llarp_ensure_client_config(std::ofstream& f, std::string basepath)
{
  llarp::Configuration def;
  llarp::Config conf;
  conf.initializeConfig(def);

  std::string confStr = def.generateINIConfig();
  f << confStr;
  f.flush();

  return true;

  /*
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

  f << "# network settings \n";
  f << "[network]\n";
  f << "profiles=" << basepath << "profiles.dat\n";
  f << "# uncomment next line to add router with pubkey to list of routers we "
       "connect directly to\n";
  f << "#strict-connect=pubkey\n";
  f << "# uncomment next line to use router with pubkey as an exit node\n";
  f << "#exit-node=pubkey\n";

  // better to set them to auto then to hard code them now
  // operating environment may change over time and this will help adapt
  // f << "ifname=auto\n";
  // f << "ifaddr=auto\n";

  // should this also be auto? or not declared?
  // probably auto in case they want to set up a hidden service
  f << "enabled=true\n";
  return true;
  */
}
