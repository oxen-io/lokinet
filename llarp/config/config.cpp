#include <config/config.hpp>

#include <config/ini.hpp>
#include <constants/defaults.hpp>
#include <net/net.hpp>
#include <util/fs.hpp>
#include <util/logger.hpp>
#include <util/logger_syslog.hpp>
#include <util/mem.hpp>
#include <util/memfn.hpp>
#include <util/str.hpp>

#include <fstream>
#include <ios>
#include <iostream>

namespace llarp
{
  bool
  RouterConfig::fromSection(string_view key, string_view val)
  {
    if(key == "netid")
    {
      if(val.size() <= NetID::size())
      {
        m_netId.assign(val.begin(), val.end());
      }
      else
      {
        llarp::LogError("invalid netid '", val, "', is too long");
      }
    }
    if(key == "max-connections")
    {
      std::string sVal(val.begin(), val.end());
      auto ival = atoi(sVal.c_str());
      if(ival > 0)
      {
        m_maxConnectedRouters = ival;
        LogInfo("max connections set to ", m_maxConnectedRouters);
      }
    }
    if(key == "min-connections")
    {
      std::string sVal(val.begin(), val.end());
      auto ival = atoi(sVal.c_str());
      if(ival > 0)
      {
        m_minConnectedRouters = ival;
        LogInfo("min connections set to ", m_minConnectedRouters);
      }
    }
    if(key == "nickname")
    {
      m_nickname.assign(val.begin(), val.end());
      // set logger name here
      LogContext::Instance().nodeName = nickname();
    }
    if(key == "encryption-privkey")
    {
      m_encryptionKeyfile.assign(val.begin(), val.end());
    }
    if(key == "contact-file")
    {
      m_ourRcFile.assign(val.begin(), val.end());
    }
    if(key == "transport-privkey")
    {
      m_transportKeyfile.assign(val.begin(), val.end());
    }
    if((key == "identity-privkey" || key == "ident-privkey"))
    {
      m_identKeyfile.assign(val.begin(), val.end());
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
      int p = atoi(std::string(val).c_str());
      // Not needed to flip upside-down - this is done in llarp::Addr(const
      // AddressInfo&)
      m_ip4addr.sin_port = p;
      m_addrInfo.port    = p;
      m_publicOverride   = true;
    }
    if(key == "worker-threads")
    {
      m_workerThreads = atoi(std::string(val).c_str());
    }
    if(key == "net-threads")
    {
      m_numNethreads = atoi(std::string(val).c_str());
      if(m_numNethreads <= 0)
      {
        m_numNethreads = 1;
      }
    }

    return true;
  }
  bool
  NetworkConfig::fromSection(string_view key, string_view val)
  {
    if(key == "profiling")
    {
      if(IsTrueValue(val))
      {
        enableProfiling.emplace(true);
      }
      else if(IsFalseValue(val))
      {
        enableProfiling.emplace(false);
      }
    }
    if(key == "profiles")
    {
      routerProfilesFile.assign(val.begin(), val.end());
      llarp::LogInfo("setting profiles to ", routerProfilesFile);
    }
    else if(key == "strict-connect")
    {
      strictConnect.assign(val.begin(), val.end());
    }
    else
    {
      netConfig.emplace(key, val);
    }
    return true;
  }

  bool
  NetdbConfig::fromSection(string_view key, string_view val)
  {
    if(key == "dir")
    {
      nodedb_dir.assign(val.begin(), val.end());
    }

    return true;
  }

  bool
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
    return true;
  }

  bool
  IwpConfig::fromSection(string_view key, string_view val)
  {
    // try IPv4 first
    uint16_t proto = 0;

    std::set< std::string > parsed_opts;
    std::string v(val.begin(), val.end());
    std::string::size_type idx;
    do
    {
      idx = v.find_first_of(',');
      if(idx != std::string::npos)
      {
        parsed_opts.insert(v.substr(0, idx));
        v = v.substr(idx + 1);
      }
      else
      {
        parsed_opts.insert(v);
      }
    } while(idx != std::string::npos);

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
    }

    if(key == "*")
    {
      m_OutboundPort = proto;
    }
    else
    {
      servers.emplace_back(key, AF_INET, proto);
    }
    return true;
  }

  bool
  ConnectConfig::fromSection(ABSL_ATTRIBUTE_UNUSED string_view key,
                             string_view val)
  {
    routers.emplace_back(val.begin(), val.end());
    return true;
  }

  bool
  ServicesConfig::fromSection(string_view key, string_view val)
  {
    services.emplace_back(std::string(key.begin(), key.end()),
                          std::string(val.begin(), val.end()));
    return true;
  }

  bool
  SystemConfig::fromSection(string_view key, string_view val)
  {
    if(key == "pidfile")
    {
      pidfile.assign(val.begin(), val.end());
    }

    return true;
  }

  bool
  MetricsConfig::fromSection(string_view key, string_view val)
  {
    if(key == "disable-metrics")
    {
      disableMetrics = true;
    }
    else if(key == "disable-metrics-log")
    {
      disableMetricLogs = true;
    }
    else if(key == "json-metrics-path")
    {
      jsonMetricsPath.assign(val.begin(), val.end());
    }
    else if(key == "metric-tank-host")
    {
      metricTankHost.assign(val.begin(), val.end());
    }
    else
    {
      // consume everything else as a metric tag
      metricTags[std::string(key)] = std::string(val);
    }

    return true;
  }

  bool
  ApiConfig::fromSection(string_view key, string_view val)
  {
    if(key == "enabled")
    {
      enableRPCServer = IsTrueValue(val);
    }
    if(key == "bind")
    {
      rpcBindAddr.assign(val.begin(), val.end());
    }
    if(key == "authkey")
    {
      // TODO: add pubkey to whitelist
    }

    return true;
  }

  bool
  LokidConfig::fromSection(string_view key, string_view val)
  {
    if(key == "service-node-seed")
    {
      usingSNSeed = true;
      ident_keyfile.assign(val.begin(), val.end());
    }
    if(key == "enabled")
    {
      whitelistRouters = IsTrueValue(val);
    }
    if(key == "jsonrpc" || key == "addr")
    {
      lokidRPCAddr.assign(val.begin(), val.end());
    }
    if(key == "username")
    {
      lokidRPCUser.assign(val.begin(), val.end());
    }
    if(key == "password")
    {
      lokidRPCPassword.assign(val.begin(), val.end());
    }

    return true;
  }

  bool
  BootstrapConfig::fromSection(string_view key, string_view val)
  {
    if(key == "add-node")
    {
      routers.emplace_back(val.begin(), val.end());
    }

    return true;
  }

  bool
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
    if(key == "type" && val == "json")
    {
      m_LogJSON = true;
    }
    if(key == "file")
    {
      LogInfo("open log file: ", val);
      std::string fname(val.begin(), val.end());
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

    return true;
  }

  template < typename Section, typename Config >
  Section
  find_section(Config &c, const std::string &name)
  {
    Section ret;

    auto visitor = [&ret](const ConfigParser::Section_t &section) -> bool {
      for(const auto &sec : section)
      {
        if(!ret.fromSection(sec.first, sec.second))
        {
          return false;
        }
      }
      return true;
    };

    if(c.VisitSection(name.c_str(), visitor))
      return ret;
    else
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
  Config::LoadFromString(string_view str)
  {
    ConfigParser parser;
    if(!parser.LoadString(str))
    {
      return false;
    }

    return parse(parser);
  }

  bool
  Config::parse(const ConfigParser &parser)
  {
    router    = find_section< RouterConfig >(parser, "router");
    network   = find_section< NetworkConfig >(parser, "network");
    connect   = find_section< ConnectConfig >(parser, "connect");
    netdb     = find_section< NetdbConfig >(parser, "netdb");
    dns       = find_section< DnsConfig >(parser, "dns");
    iwp_links = find_section< IwpConfig >(parser, "bind");
    services  = find_section< ServicesConfig >(parser, "services");
    system    = find_section< SystemConfig >(parser, "system");
    metrics   = find_section< MetricsConfig >(parser, "metrics");
    api       = find_section< ApiConfig >(parser, "api");
    lokid     = find_section< LokidConfig >(parser, "lokid");
    bootstrap = find_section< BootstrapConfig >(parser, "bootstrap");
    logging   = find_section< LoggingConfig >(parser, "logging");
    return true;
  }

}  // namespace llarp

/// fname should be a relative path (from CWD) or absolute path to the config
/// file
extern "C" bool
llarp_ensure_config(const char *fname, const char *basedir, bool overwrite,
                    bool asRouter)
{
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

  std::string basepath = "";
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
  llarp_generic_ensure_config(f, basepath);
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
llarp_generic_ensure_config(std::ofstream &f, std::string basepath)
{
  f << "# this configuration was auto generated with 'sane' defaults"
    << std::endl;
  f << "# change these values as desired" << std::endl;
  f << std::endl << std::endl;
  f << "[router]" << std::endl;
  f << "# number of crypto worker threads " << std::endl;
  f << "threads=4" << std::endl;
  f << "# path to store signed RC" << std::endl;
  f << "contact-file=" << basepath << "self.signed" << std::endl;
  f << "# path to store transport private key" << std::endl;
  f << "transport-privkey=" << basepath << "transport.private" << std::endl;
  f << "# path to store identity signing key" << std::endl;
  f << "ident-privkey=" << basepath << "identity.private" << std::endl;
  f << "# encryption key for onion routing" << std::endl;
  f << "encryption-privkey=" << basepath << "encryption.private" << std::endl;
  f << std::endl;
  f << "# uncomment following line to set router nickname to 'lokinet'"
    << std::endl;
  f << "#nickname=lokinet" << std::endl;
  f << std::endl << std::endl;

  // logging
  f << "[logging]" << std::endl;
  f << "level=info" << std::endl;
  f << "# uncomment for logging to file" << std::endl;
  f << "#type=file" << std::endl;
  f << "#file=/path/to/logfile" << std::endl;
  f << "# uncomment for syslog logging" << std::endl;
  f << "#type=syslog" << std::endl;

  // metrics
  f << "[metrics]" << std::endl;
  f << "json-metrics-path=" << basepath << "metrics.json" << std::endl;

  f << std::endl << std::endl;

  f << "# admin api (disabled by default)" << std::endl;
  f << "[api]" << std::endl;
  f << "enabled=false" << std::endl;
  f << "#authkey=insertpubkey1here" << std::endl;
  f << "#authkey=insertpubkey2here" << std::endl;
  f << "#authkey=insertpubkey3here" << std::endl;
  f << "bind=127.0.0.1:1190" << std::endl;
  f << std::endl << std::endl;

  f << "# system settings for privileges and such" << std::endl;
  f << "[system]" << std::endl;
  f << "user=" << DEFAULT_LOKINET_USER << std::endl;
  f << "group=" << DEFAULT_LOKINET_GROUP << std::endl;
  f << "pidfile=" << basepath << "lokinet.pid" << std::endl;
  f << std::endl << std::endl;

  f << "# dns provider configuration section" << std::endl;
  f << "[dns]" << std::endl;
  f << "# resolver" << std::endl;
  f << "upstream=" << DEFAULT_RESOLVER_US << std::endl;

// Make auto-config smarter
// will this break reproducibility rules?
// (probably)
#ifdef __linux__
#ifdef ANDROID
  f << "bind=127.0.0.1:1153" << std::endl;
#else
  f << "bind=127.3.2.1:53" << std::endl;
#endif
#else
  f << "bind=127.0.0.1:53" << std::endl;
#endif
  f << std::endl << std::endl;

  f << "# network database settings block " << std::endl;
  f << "[netdb]" << std::endl;
  f << "# directory for network database skiplist storage" << std::endl;
  f << "dir=" << basepath << "netdb" << std::endl;
  f << std::endl << std::endl;

  f << "# bootstrap settings" << std::endl;
  f << "[bootstrap]" << std::endl;
  f << "# add a bootstrap node's signed identity to the list of nodes we want "
       "to bootstrap from"
    << std::endl;
  f << "# if we don't have any peers we connect to this router" << std::endl;
  f << "add-node=" << basepath << "bootstrap.signed" << std::endl;
  // we only process one of these...
  // f << "# add another bootstrap node" << std::endl;
  // f << "#add-node=/path/to/alternative/self.signed" << std::endl;
  f << std::endl << std::endl;
}

void
llarp_ensure_router_config(std::ofstream &f, std::string basepath)
{
  f << "# lokid settings (disabled by default)" << std::endl;
  f << "[lokid]" << std::endl;
  f << "enabled=false" << std::endl;
  f << "jsonrpc=127.0.0.1:22023" << std::endl;
  f << "#service-node-seed=/path/to/servicenode/seed" << std::endl;
  f << std::endl;
  f << "# network settings " << std::endl;
  f << "[network]" << std::endl;
  f << "profiles=" << basepath << "profiles.dat" << std::endl;
  // better to let the routers auto-configure
  // f << "ifaddr=auto" << std::endl;
  // f << "ifname=auto" << std::endl;
  f << "enabled=true" << std::endl;
  f << "exit=false" << std::endl;
  f << "#exit-blacklist=tcp:25" << std::endl;
  f << "#exit-whitelist=tcp:*" << std::endl;
  f << "#exit-whitelist=udp:*" << std::endl;
  f << std::endl;
  f << "# ROUTERS ONLY: publish network interfaces for handling inbound traffic"
    << std::endl;
  f << "[bind]" << std::endl;
  // get ifname
  std::string ifname;
  if(llarp::GetBestNetIF(ifname, AF_INET))
    f << ifname << "=1090" << std::endl;
  else
    f << "# could not autodetect network interface" << std::endl
      << "#eth0=1090" << std::endl;

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
      return false;
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
      example_f << "# this is an example configuration for a snapp"
                << std::endl;
      example_f << "[example-snapp]" << std::endl;
      example_f << "# keyfile is the path to the private key of the snapp, "
                   "your .loki is tied to this key, DON'T LOSE IT"
                << std::endl;
      example_f << "keyfile=" << basepath << "example-snap-keyfile.private"
                << std::endl;
      example_f << "# ifaddr is the ip range to allocate to this snapp"
                << std::endl;
      example_f << "ifaddr=" << ip << std::endl;
      // probably fine to leave this (and not-auto-detect it) I'm not worried
      // about any collisions
      example_f << "# ifname is the name to try and give to the network "
                   "interface this snap owns"
                << std::endl;
      example_f << "ifname=snapp-tun0" << std::endl;
    }
    else
    {
      llarp::LogError("failed to write ", snappExample_fpath);
    }
  }
  // now do up fname
  f << std::endl << std::endl;
  f << "# snapps configuration section" << std::endl;
  f << "[services]";
  f << "# uncomment next line to enable a snapp" << std::endl;
  f << "#example-snapp=" << snappExample_fpath << std::endl;
  f << std::endl << std::endl;

  f << "# network settings " << std::endl;
  f << "[network]" << std::endl;
  f << "profiles=" << basepath << "profiles.dat" << std::endl;
  f << "# uncomment next line to add router with pubkey to list of routers we "
       "connect directly to"
    << std::endl;
  f << "#strict-connect=pubkey" << std::endl;
  f << "# uncomment next line to use router with pubkey as an exit node"
    << std::endl;
  f << "#exit-node=pubkey" << std::endl;

  // better to set them to auto then to hard code them now
  // operating environment may change over time and this will help adapt
  // f << "ifname=auto" << std::endl;
  // f << "ifaddr=auto" << std::endl;

  // should this also be auto? or not declared?
  // probably auto in case they want to set up a hidden service
  f << "enabled=true" << std::endl;
  return true;
}
