#include <config/config.hpp>

#include <config/ini.hpp>
#include <constants/defaults.hpp>
#include <constants/limits.hpp>
#include <net/net.hpp>
#include <router_contact.hpp>
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

namespace llarp
{
  const char *
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
    if(const char *ptr = lokinetEnv(envNameSuffix))
      return ptr;
    return {val.begin(), val.end()};
  }

  int
  fromEnv(const int &val, string_view envNameSuffix)
  {
    if(const char *ptr = lokinetEnv(envNameSuffix))
      return std::atoi(ptr);
    return val;
  }

  uint16_t
  fromEnv(const uint16_t &val, string_view envNameSuffix)
  {
    if(const char *ptr = lokinetEnv(envNameSuffix))
      return std::atoi(ptr);

    return val;
  }

  size_t
  fromEnv(const size_t &val, string_view envNameSuffix)
  {
    if(const char *ptr = lokinetEnv(envNameSuffix))
      return std::atoll(ptr);

    return val;
  }

  nonstd::optional< bool >
  fromEnv(const nonstd::optional< bool > &val, string_view envNameSuffix)
  {
    if(const char *ptr = lokinetEnv(envNameSuffix))
      return IsTrueValue(ptr);

    return val;
  }

  int
  svtoi(string_view val)
  {
    return std::atoi(val.data());
  }

  nonstd::optional< bool >
  setOptBool(string_view val)
  {
    if(IsTrueValue(val))
    {
      return true;
    }
    else if(IsFalseValue(val))
    {
      return false;
    }
    return {};
  }

  void
  RouterConfig::fromSection(string_view key, string_view val)
  {
    if(key == "job-queue-size")
    {
      auto sval = svtoi(val);
      if(sval >= 1024)
      {
        m_JobQueueSize = sval;
        LogInfo("Set job queue size to ", m_JobQueueSize);
      }
    }
    if(key == "default-protocol")
    {
      m_DefaultLinkProto = val;
      LogInfo("overriding default link protocol to '", val, "'");
    }
    if(key == "netid")
    {
      if(val.size() <= NetID::size())
      {
        m_netId = val;
        LogInfo("setting netid to '", val, "'");
      }
      else
      {
        llarp::LogError("invalid netid '", val, "', is too long");
      }
    }
    if(key == "max-connections")
    {
      auto ival = svtoi(val);
      if(ival > 0)
      {
        m_maxConnectedRouters = ival;
        LogInfo("max connections set to ", m_maxConnectedRouters);
      }
    }
    if(key == "min-connections")
    {
      auto ival = svtoi(val);
      if(ival > 0)
      {
        m_minConnectedRouters = ival;
        LogInfo("min connections set to ", m_minConnectedRouters);
      }
    }
    if(key == "nickname")
    {
      m_nickname = val;
      // set logger name here
      LogContext::Instance().nodeName = nickname();
      LogInfo("nickname set");
    }
    if(key == "encryption-privkey")
    {
      m_encryptionKeyfile = val;
      LogDebug("encryption key set to ", m_encryptionKeyfile);
    }
    if(key == "contact-file")
    {
      m_ourRcFile = val;
      LogDebug("rc file set to ", m_ourRcFile);
    }
    if(key == "transport-privkey")
    {
      m_transportKeyfile = val;
      LogDebug("transport key set to ", m_transportKeyfile);
    }
    if((key == "identity-privkey" || key == "ident-privkey"))
    {
      m_identKeyfile = val;
      LogDebug("identity key set to ", m_identKeyfile);
    }
    if(key == "public-address" || key == "public-ip")
    {
      llarp::LogInfo("public ip ", val, " size ", val.size());
      if(val.size() < 17)
      {
        // assume IPv4
        llarp::Addr a(val);
        llarp::LogInfo("setting public ipv4 ", a);
        m_addrInfo.ip    = *a.addr6();
        m_publicOverride = true;
      }
    }
    if(key == "public-port")
    {
      llarp::LogInfo("Setting public port ", val);
      int p = svtoi(val);
      // Not needed to flip upside-down - this is done in llarp::Addr(const
      // AddressInfo&)
      m_ip4addr.sin_port = p;
      m_addrInfo.port    = p;
      m_publicOverride   = true;
    }
    if(key == "worker-threads" || key == "threads")
    {
      m_workerThreads = svtoi(val);
      if(m_workerThreads <= 0)
      {
        LogWarn("worker threads invalid value: '", val, "' defaulting to 1");
        m_workerThreads = 1;
      }
      else
      {
        LogDebug("set to use ", m_workerThreads, " worker threads");
      }
    }
    if(key == "net-threads")
    {
      m_numNetThreads = svtoi(val);
      if(m_numNetThreads <= 0)
      {
        LogWarn("net threads invalid value: '", val, "' defaulting to 1");
        m_numNetThreads = 1;
      }
      else
      {
        LogDebug("set to use ", m_numNetThreads, " net threads");
      }
    }
    if(key == "block-bogons")
    {
      m_blockBogons = setOptBool(val);
    }
  }

  void
  NetworkConfig::fromSection(string_view key, string_view val)
  {
    if(key == "profiling")
    {
      m_enableProfiling = setOptBool(val);
    }
    else if(key == "profiles")
    {
      m_routerProfilesFile = val;
      llarp::LogInfo("setting profiles to ", routerProfilesFile());
    }
    else if(key == "strict-connect")
    {
      m_strictConnect = val;
    }
    else
    {
      m_netConfig.emplace(key, val);
    }
  }

  void
  NetdbConfig::fromSection(string_view key, string_view val)
  {
    if(key == "dir")
    {
      m_nodedbDir = val;
    }
  }

  void
  DnsConfig::fromSection(string_view key, string_view val)
  {
    if(key == "upstream")
    {
      llarp::LogInfo("add upstream resolver ", val);
      netConfig.emplace("upstream-dns", val);
    }
    if(key == "bind")
    {
      llarp::LogInfo("set local dns to ", val);
      netConfig.emplace("local-dns", val);
    }
  }

  void
  LinksConfig::fromSection(string_view key, string_view val)
  {
    uint16_t proto = 0;

    std::unordered_set< std::string > parsed_opts;
    std::string::size_type idx;
    static constexpr char delimiter = ',';
    do
    {
      idx = val.find_first_of(delimiter);
      if(idx != string_view::npos)
      {
        parsed_opts.insert(TrimWhiteSpace(val.substr(0, idx)));
        val.remove_prefix(idx + 1);
      }
      else
      {
        parsed_opts.insert(TrimWhiteSpace(val));
      }
    } while(idx != string_view::npos);
    std::unordered_set< std::string > opts;
    /// for each option
    for(const auto &item : parsed_opts)
    {
      /// see if it's a number
      auto port = std::atoi(item.c_str());
      if(port > 0)
      {
        /// set port
        if(proto == 0)
        {
          proto = port;
        }
      }
      else
      {
        opts.insert(item);
      }
    }

    if(key == "*")
    {
      m_OutboundLink = std::make_tuple(
          "*", AF_INET, fromEnv(proto, "OUTBOUND_PORT"), std::move(opts));
    }
    else
    {
      m_InboundLinks.emplace_back(key, AF_INET, proto, std::move(opts));
    }
  }

  void
  ConnectConfig::fromSection(string_view /*key*/, string_view val)
  {
    routers.emplace_back(val.begin(), val.end());
  }

  void
  ServicesConfig::fromSection(string_view key, string_view val)
  {
    services.emplace_back(key, val);
  }

  void
  SystemConfig::fromSection(string_view key, string_view val)
  {
    if(key == "pidfile")
    {
      pidfile = val;
    }
  }

  void
  ApiConfig::fromSection(string_view key, string_view val)
  {
    if(key == "enabled")
    {
      m_enableRPCServer = IsTrueValue(val);
    }
    if(key == "bind")
    {
      m_rpcBindAddr = val;
    }
    if(key == "authkey")
    {
      // TODO: add pubkey to whitelist
    }
  }

  void
  LokidConfig::fromSection(string_view key, string_view val)
  {
    if(key == "service-node-seed")
    {
      usingSNSeed   = true;
      ident_keyfile = std::string{val};
    }
    if(key == "enabled")
    {
      whitelistRouters = IsTrueValue(val);
    }
    if(key == "jsonrpc" || key == "addr")
    {
      lokidRPCAddr = val;
    }
    if(key == "username")
    {
      lokidRPCUser = val;
    }
    if(key == "password")
    {
      lokidRPCPassword = val;
    }
  }

  void
  BootstrapConfig::fromSection(string_view key, string_view val)
  {
    if(key == "add-node")
    {
      routers.emplace_back(val.begin(), val.end());
    }
  }

  void
  LoggingConfig::fromSection(string_view key, string_view val)
  {
    if(key == "type" && val == "syslog")
    {
      // TODO(despair): write event log syslog class
#if defined(_WIN32)
      LogError("syslog not supported on win32");
#else
      LogInfo("Switching to syslog");
      LogContext::Instance().logStream = std::make_unique< SysLogStream >();
#endif
    }
    if(key == "level")
    {
      const auto maybe = LogLevelFromString(val);
      if(not maybe.has_value())
      {
        LogError("bad log level: ", val);
        return;
      }
      const LogLevel lvl                  = maybe.value();
      LogContext::Instance().runtimeLevel = lvl;
      LogInfo("Log level set to ", LogLevelToName(lvl));
    }
    if(key == "type" && val == "json")
    {
      m_LogJSON = true;
    }
    if(key == "file")
    {
      LogInfo("open log file: ", val);
      std::string fname   = val;
      FILE *const logfile = ::fopen(fname.c_str(), "a");
      if(logfile)
      {
        m_LogFile = logfile;
        LogInfo("will log to file ", val);
      }
      else if(errno)
      {
        LogError("could not open log file at '", val, "': ", strerror(errno));
        errno = 0;
      }
      else
      {
        LogError("failed to open log file at '", val,
                 "' for an unknown reason, bailing tf out kbai");
        ::abort();
      }
    }
  }

  template < typename Section, typename Config >
  Section
  find_section(Config &c, const std::string &name)
  {
    Section ret;

    auto visitor = [&ret](const ConfigParser::Section_t &section) -> bool {
      for(const auto &sec : section)
      {
        ret.fromSection(sec.first, sec.second);
      }
      return true;
    };

    if(c.VisitSection(name.c_str(), visitor))
    {
      return ret;
    }

    return {};
  }

  bool
  Config::Load(const char *fname)
  {
    ConfigParser parser;
    if(!parser.LoadFile(fname))
    {
      return false;
    }

    return parse(parser);
  }

  bool
  Config::LoadFromStr(string_view str)
  {
    ConfigParser parser;
    if(!parser.LoadFromStr(str))
    {
      return false;
    }

    return parse(parser);
  }

  bool
  Config::parse(const ConfigParser &parser)
  {
    if(Lokinet_INIT())
      return false;
    router    = find_section< RouterConfig >(parser, "router");
    network   = find_section< NetworkConfig >(parser, "network");
    connect   = find_section< ConnectConfig >(parser, "connect");
    netdb     = find_section< NetdbConfig >(parser, "netdb");
    dns       = find_section< DnsConfig >(parser, "dns");
    links     = find_section< LinksConfig >(parser, "bind");
    services  = find_section< ServicesConfig >(parser, "services");
    system    = find_section< SystemConfig >(parser, "system");
    api       = find_section< ApiConfig >(parser, "api");
    lokid     = find_section< LokidConfig >(parser, "lokid");
    bootstrap = find_section< BootstrapConfig >(parser, "bootstrap");
    logging   = find_section< LoggingConfig >(parser, "logging");
    return true;
  }

  fs::path
  GetDefaultConfigDir()
  {
#ifdef _WIN32
    const fs::path homedir = fs::path(getenv("APPDATA"));
#else
    const fs::path homedir = fs::path(getenv("HOME"));
#endif
    return homedir / fs::path(".lokinet");
  }

  fs::path
  GetDefaultConfigPath()
  {
    return GetDefaultConfigDir() / "lokinet.ini";
  }

}  // namespace llarp

/// fname should be a relative path (from CWD) or absolute path to the config
/// file
extern "C" bool
llarp_ensure_config(const char *fname, const char *basedir, bool overwrite,
                    bool asRouter)
{
  if(Lokinet_INIT())
    return false;
  std::error_code ec;
  if(fs::exists(fname, ec) && !overwrite)
  {
    return true;
  }
  if(ec)
  {
    llarp::LogError(ec);
    return false;
  }

  std::string basepath;
  if(basedir)
  {
    basepath = basedir;
#ifndef _WIN32
    basepath += "/";
#else
    basepath += "\\";
#endif
  }

  llarp::LogInfo("Attempting to create config file ", fname);

  // abort if config already exists
  if(!asRouter)
  {
    if(fs::exists(fname, ec) && !overwrite)
    {
      llarp::LogError(fname, " currently exists, please use -f to overwrite");
      return true;
    }
    if(ec)
    {
      llarp::LogError(ec);
      return false;
    }
  }

  // write fname ini
  auto optional_f =
      llarp::util::OpenFileStream< std::ofstream >(fname, std::ios::binary);
  if(!optional_f || !optional_f.value().is_open())
  {
    llarp::LogError("failed to open ", fname, " for writing");
    return false;
  }
  auto &f = optional_f.value();
  llarp_generic_ensure_config(f, basepath, asRouter);
  if(asRouter)
  {
    llarp_ensure_router_config(f, basepath);
  }
  else
  {
    llarp_ensure_client_config(f, basepath);
  }
  llarp::LogInfo("Generated new config ", fname);
  return true;
}

void
llarp_generic_ensure_config(std::ofstream &f, std::string basepath,
                            bool isRouter)
{
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
  f << "# uncomment following line to set router nickname to 'lokinet'"
    << std::endl;
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
}

void
llarp_ensure_router_config(std::ofstream &f, std::string basepath)
{
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
  if(llarp::GetBestNetIF(ifname, AF_INET))
  {
    f << ifname << "=1090\n";
  }
  else
  {
    f << "# could not autodetect network interface\n"
      << "#eth0=1090\n";
  }

  f << std::endl;
}

bool
llarp_ensure_client_config(std::ofstream &f, std::string basepath)
{
  // write snapp-example.ini
  const std::string snappExample_fpath = basepath + "snapp-example.ini";
  {
    auto stream = llarp::util::OpenFileStream< std::ofstream >(
        snappExample_fpath, std::ios::binary);
    if(!stream)
    {
      return false;
    }
    auto &example_f = stream.value();
    if(example_f.is_open())
    {
      // pick ip
      // don't revert me
      const static std::string ip = "10.33.0.1/16";
      /*
      std::string ip = llarp::findFreePrivateRange();
      if(ip == "")
      {
        llarp::LogError(
            "Couldn't easily detect a private range to map lokinet onto");
        return false;
      }
     */
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
}
