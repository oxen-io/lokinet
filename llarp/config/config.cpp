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

  int
  svtoi(string_view val)
  {
    return std::atoi(val.data());
  }

  nonstd::optional<bool>
  setOptBool(string_view val)
  {
    if (IsTrueValue(val))
    {
      return true;
    }
    else if (IsFalseValue(val))
    {
      return false;
    }
    return {};
  }

  bool
  RouterConfig::parseSectionValues(const ConfigParser& parser, const SectionValues_t& values)
  {
    // [router]:job-queue-size
    auto parsedValue = parser.getSingleSectionValue(values, "router", "job-queue-size", false);
    if (not parsedValue.empty()) 
    {
      int val = svtoi(parsedValue);
      if (val < 1024)
        throw std::invalid_argument("invalid value for [router]:job-queue-size, must be 1024 or greater");
      else
        m_JobQueueSize = val;
    }

    // [router]:default-protocol
    parsedValue = parser.getSingleSectionValue(values, "router", "default-protocol", false);
    if (not parsedValue.empty())
      m_DefaultLinkProto = parsedValue;

    // [router]:netid
    parsedValue = parser.getSingleSectionValue(values, "router", "netid", true);
    assert(not parsedValue.empty()); // gauranteed by getSingleSectionValue() with required == true
    if(parsedValue.size() > NetID::size())
      throw std::invalid_argument("value for [router]:netid is too long");
    m_netId = str(parsedValue);

    // [router]:max-connections
    parsedValue = parser.getSingleSectionValue(values, "router", "max-connections", false);
    if (not parsedValue.empty())
    {
      int val = svtoi(parsedValue);
      if (val < 1)
        throw std::invalid_argument("invalid value for [router]:max-connections");
      else
        m_maxConnectedRouters = val;
    }

    // [router]:min-connections
    parsedValue = parser.getSingleSectionValue(values, "router", "min-connections", false);
    if (not parsedValue.empty())
    {
      int val = svtoi(parsedValue);
      if (val < 1)
        throw std::invalid_argument("invalid value for [router]:min-connections");
      else
        m_minConnectedRouters = val;
    }

    // additional check that min <= max
    if (m_minConnectedRouters > m_maxConnectedRouters)
      throw std::invalid_argument("[router]:min-connections must be less than [router]:max-connections");

    // [router]:nickname
    parsedValue = parser.getSingleSectionValue(values, "router", "nickname", false);
    if (not parsedValue.empty())
    {
      m_nickname = str(parsedValue);
      // TODO: side effect here, no side effects in config parsing!!
      LogContext::Instance().nodeName = nickname();
    }

    // [router]:encryption-privkey
    parsedValue = parser.getSingleSectionValue(values, "router", "encryption-privkey", false);
    if (not parsedValue.empty())
      m_encryptionKeyfile = str(parsedValue);

    // [router]:contact-file
    parsedValue = parser.getSingleSectionValue(values, "router", "contact-file", false);
    if (not parsedValue.empty())
      m_ourRcFile = str(parsedValue);

    // [router]:transport-privkey
    parsedValue = parser.getSingleSectionValue(values, "router", "transport-privkey", false);
    if (not parsedValue.empty())
      m_transportKeyfile = str(parsedValue);

    // [router]:identity-privkey OR
    // [router]:ident-privkey
    // apparently loki-launcher made its own config files at one point and typoed this,
    // so we support both
    parsedValue = parser.getSingleSectionValue(values, "router", "identity-privkey", false);
    if (parsedValue.empty()) 
      parsedValue = parser.getSingleSectionValue(values, "router", "ident-privkey", false);
    if (not parsedValue.empty())
      m_identKeyfile = str(parsedValue);

    // [router]:public-address OR
    // [router]:public-ip
    // apparently loki-launcher made its own config files at one point and typoed this,
    // so we support both
    parsedValue = parser.getSingleSectionValue(values, "router", "public-address", false);
    if (parsedValue.empty()) 
      parsedValue = parser.getSingleSectionValue(values, "router", "public-ip", false);
    if (not parsedValue.empty())
    {
      llarp::LogInfo("public ip ", parsedValue, " size ", parsedValue.size());
      if(parsedValue.size() < 17)
      {
        // assume IPv4
        llarp::Addr a(parsedValue);
        llarp::LogInfo("setting public ipv4 ", a);
        m_addrInfo.ip = *a.addr6();
        m_publicOverride = true;
      }
    }

    // [router]:public-port
    parsedValue = parser.getSingleSectionValue(values, "router", "public-port", false);
    if (not parsedValue.empty()) 
    {
      llarp::LogInfo("Setting public port ", parsedValue);
      int p = svtoi(parsedValue);
      // Not needed to flip upside-down - this is done in llarp::Addr(const
      // AddressInfo&)
      m_ip4addr.sin_port = p;
      m_addrInfo.port = p;
      m_publicOverride = true;
    }

    // [router]:worker-threads OR
    // [router]:threads
    // apparently loki-launcher made its own config files at one point and typoed this,
    // so we support both
    parsedValue = parser.getSingleSectionValue(values, "router", "worker-threads", false);
    if (parsedValue.empty()) 
      parsedValue = parser.getSingleSectionValue(values, "router", "threads", false);
    if (not parsedValue.empty())
    {
      int val = svtoi(parsedValue);
      if(val <= 0)
        throw std::invalid_argument("invalid value for [router]:worker-threads");
      else
        m_workerThreads = val;
    }

    // [router]:public-port
    parsedValue = parser.getSingleSectionValue(values, "router", "public-port", false);
    if (not parsedValue.empty())
    {
      int val = svtoi(parsedValue);
      if (val <= 0)
        throw std::invalid_argument("invalid value for [router]:public-port");
      else
        m_numNetThreads = val;
    }

    // [router]:block-bogons
    parsedValue = parser.getSingleSectionValue(values, "router", "block-bogons", false);
    if (not parsedValue.empty())
    {
      auto val = setOptBool(parsedValue);
      if (not val.has_value())
        throw std::invalid_argument("invalid value for [router]:block-bogons");
      else
        m_blockBogons = val;
    }

    return true;
  }

  bool
  NetworkConfig::parseSectionValues(const ConfigParser& parser, const SectionValues_t& values)
  {
    /*
    if(key == "profiling")
    {
      m_enableProfiling = setOptBool(val);
    }
    else if (key == "profiles")
    {
      m_routerProfilesFile = str(val);
      llarp::LogInfo("setting profiles to ", routerProfilesFile());
    }
    else if (key == "strict-connect")
    {
      m_strictConnect = str(val);
    }
    else
    {
      m_netConfig.emplace(str(key), str(val));  // str()'s here for gcc 5 compat
    }
    */

    return true;
  }

  bool
  NetdbConfig::parseSectionValues(const ConfigParser& parser, const SectionValues_t& values)
  {
    /*
    if(key == "dir")
    {
      m_nodedbDir = str(val);
    }
    */
    return true;
  }

  bool
  DnsConfig::parseSectionValues(const ConfigParser& parser, const SectionValues_t& values)
  {
    /*
    if(key == "upstream")
    {
      llarp::LogInfo("add upstream resolver ", val);
      netConfig.emplace("upstream-dns", str(val));  // str() for gcc 5 compat
    }
    if (key == "bind")
    {
      llarp::LogInfo("set local dns to ", val);
      netConfig.emplace("local-dns", str(val));  // str() for gcc 5 compat
    }
    */
    return true;
  }

  bool
  LinksConfig::parseSectionValues(const ConfigParser& parser, const SectionValues_t& values)
  {
    /*
    uint16_t proto = 0;

    std::unordered_set<std::string> parsed_opts;
    std::string::size_type idx;
    static constexpr char delimiter = ',';
    do
    {
      idx = val.find_first_of(delimiter);
      if (idx != string_view::npos)
      {
        parsed_opts.emplace(TrimWhitespace(val.substr(0, idx)));
        val.remove_prefix(idx + 1);
      }
      else
      {
        parsed_opts.emplace(TrimWhitespace(val));
      }
    } while (idx != string_view::npos);
    std::unordered_set<std::string> opts;
    /// for each option
    for (const auto& item : parsed_opts)
    {
      /// see if it's a number
      auto port = std::atoi(item.c_str());
      if (port > 0)
      {
        /// set port
        if (proto == 0)
        {
          proto = port;
        }
      }
      else
      {
        opts.insert(item);
      }
    }

    if (key == "*")
    {
      m_OutboundLink =
          std::make_tuple("*", AF_INET, fromEnv(proto, "OUTBOUND_PORT"), std::move(opts));
    }
    else
    {
      // str() here for gcc 5 compat
      m_InboundLinks.emplace_back(str(key), AF_INET, proto, std::move(opts));
    }
    */
    return true;
  }

  bool
  ConnectConfig::parseSectionValues(const ConfigParser& parser, const SectionValues_t& values)
  {
    // routers.emplace_back(val.begin(), val.end());
    return true;
  }

  bool
  ServicesConfig::parseSectionValues(const ConfigParser& parser, const SectionValues_t& values)
  {
    // services.emplace_back(str(key), str(val));  // str()'s here for gcc 5 compat
    return true;
  }

  bool
  SystemConfig::parseSectionValues(const ConfigParser& parser, const SectionValues_t& values)
  {
    /*
    if(key == "pidfile")
    {
      pidfile = str(val);
    }
    */
    return true;
  }

  bool
  ApiConfig::parseSectionValues(const ConfigParser& parser, const SectionValues_t& values)
  {
    /*
    if(key == "enabled")
    {
      m_enableRPCServer = IsTrueValue(val);
    }
    if (key == "bind")
    {
      m_rpcBindAddr = str(val);
    }
    if (key == "authkey")
    {
      // TODO: add pubkey to whitelist
    }
    */
    return true;
  }

  bool
  LokidConfig::parseSectionValues(const ConfigParser& parser, const SectionValues_t& values)
  {
    /*
    if(key == "service-node-seed")
    {
      usingSNSeed = true;
      ident_keyfile = std::string{val};
    }
    if (key == "enabled")
    {
      whitelistRouters = IsTrueValue(val);
    }
    if (key == "jsonrpc" || key == "addr")
    {
      lokidRPCAddr = str(val);
    }
    if (key == "username")
    {
      lokidRPCUser = str(val);
    }
    if (key == "password")
    {
      lokidRPCPassword = str(val);
    }
    */
    return true;
  }

  bool
  BootstrapConfig::parseSectionValues(const ConfigParser& parser, const SectionValues_t& values)
  {
    /*
    if(key == "add-node")
    {
      routers.emplace_back(val.begin(), val.end());
    }
    */
    return true;
  }

  bool
  LoggingConfig::parseSectionValues(const ConfigParser& parser, const SectionValues_t& values)
  {
    /*
    if(key == "type" && val == "syslog")
    {
      // TODO(despair): write event log syslog class
#if defined(_WIN32)
      LogError("syslog not supported on win32");
#else
      LogInfo("Switching to syslog");
      LogContext::Instance().logStream = std::make_unique<SysLogStream>();
#endif
    }
    if (key == "level")
    {
      const auto maybe = LogLevelFromString(str(val));
      if (not maybe.has_value())
      {
        LogError("bad log level: ", val);
        return;
      }
      const LogLevel lvl = maybe.value();
      LogContext::Instance().runtimeLevel = lvl;
      LogInfo("Log level set to ", LogLevelToName(lvl));
    }
    if (key == "type" && val == "json")
    {
      m_LogJSON = true;
    }
    if (key == "file")
    {
      LogInfo("open log file: ", val);
      std::string fname{val};
      FILE* const logfile = ::fopen(fname.c_str(), "a");
      if (logfile)
      {
        m_LogFile = logfile;
        LogInfo("will log to file ", val);
      }
      else if (errno)
      {
        LogError("could not open log file at '", val, "': ", strerror(errno));
        errno = 0;
      }
      else
      {
        LogError(
            "failed to open log file at '", val, "' for an unknown reason, bailing tf out kbai");
        ::abort();
      }
    }
    */
    return true;
  }

  template < typename Section >
  Section
  find_section(const ConfigParser &parser, const std::string &name)
  {
    Section section;

    auto visitor = [&](const ConfigParser::SectionValues_t& sectionValues) {
      return section.parseSectionValues(parser, sectionValues);
    };

    // TODO: exceptions, please. fuck.
    //       parser.VisitSection just passes-through the return value of our
    //       lambda from above
    if(parser.VisitSection(name.c_str(), visitor))
    {
      return section;
    }

    return {};
  }

  bool
  Config::Load(const char* fname)
  {
    ConfigParser parser;
    if (!parser.LoadFile(fname))
    {
      return false;
    }

    return parse(parser);
  }

  bool
  Config::LoadFromStr(string_view str)
  {
    ConfigParser parser;
    if (!parser.LoadFromStr(str))
    {
      return false;
    }

    return parse(parser);
  }

  bool
  Config::parse(const ConfigParser& parser)
  {
    if (Lokinet_INIT())
      return false;
    router = find_section<RouterConfig>(parser, "router");
    network = find_section<NetworkConfig>(parser, "network");
    connect = find_section<ConnectConfig>(parser, "connect");
    netdb = find_section<NetdbConfig>(parser, "netdb");
    dns = find_section<DnsConfig>(parser, "dns");
    links = find_section<LinksConfig>(parser, "bind");
    services = find_section<ServicesConfig>(parser, "services");
    system = find_section<SystemConfig>(parser, "system");
    api = find_section<ApiConfig>(parser, "api");
    lokid = find_section<LokidConfig>(parser, "lokid");
    bootstrap = find_section<BootstrapConfig>(parser, "bootstrap");
    logging = find_section<LoggingConfig>(parser, "logging");
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
llarp_ensure_config(const char* fname, const char* basedir, bool overwrite, bool asRouter)
{
  if (Lokinet_INIT())
    return false;
  std::error_code ec;
  if (fs::exists(fname, ec) && !overwrite)
  {
    return true;
  }
  if (ec)
  {
    llarp::LogError(ec);
    return false;
  }

  std::string basepath;
  if (basedir)
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
  if (!asRouter)
  {
    if (fs::exists(fname, ec) && !overwrite)
    {
      llarp::LogError(fname, " currently exists, please use -f to overwrite");
      return true;
    }
    if (ec)
    {
      llarp::LogError(ec);
      return false;
    }
  }

  // write fname ini
  auto optional_f = llarp::util::OpenFileStream<std::ofstream>(fname, std::ios::binary);
  if (!optional_f || !optional_f.value().is_open())
  {
    llarp::LogError("failed to open ", fname, " for writing");
    return false;
  }
  auto& f = optional_f.value();
  llarp_generic_ensure_config(f, basepath, asRouter);
  if (asRouter)
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
llarp_generic_ensure_config(std::ofstream& f, std::string basepath, bool isRouter)
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
}

void
llarp_ensure_router_config(std::ofstream& f, std::string basepath)
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
}

bool
llarp_ensure_client_config(std::ofstream& f, std::string basepath)
{
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
