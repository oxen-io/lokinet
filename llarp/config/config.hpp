#ifndef LLARP_CONFIG_HPP
#define LLARP_CONFIG_HPP

#include <crypto/types.hpp>
#include <router_contact.hpp>
#include <util/fs.hpp>
#include <util/str.hpp>
#include <config/ini.hpp>
#include <config/definition.hpp>

#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>

namespace llarp
{
  using SectionValues_t = llarp::ConfigParser::SectionValues_t;
  using Config_impl_t = llarp::ConfigParser::Config_impl_t;

  // TODO: don't use these maps. they're sloppy and difficult to follow
  using FreehandOptions = std::unordered_multimap< std::string, std::string >;

  /// Small struct to gather all parameters needed for config generation to reduce the number of
  /// parameters that need to be passed around.
  struct ConfigGenParameters
  {
    bool isRelay = false;
    fs::path defaultDataDir;
  };

  struct RouterConfig
  {
    size_t m_minConnectedRouters;
    size_t m_maxConnectedRouters;

    std::string m_netId;
    std::string m_nickname;

    std::string m_dataDir;

    bool m_blockBogons;

    bool m_publicOverride = false;
    struct sockaddr_in m_ip4addr;
    AddressInfo m_addrInfo;

    int m_workerThreads;
    int m_numNetThreads;

    size_t m_JobQueueSize;

    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct NetworkConfig
  {
    nonstd::optional< bool > m_enableProfiling;
    std::string m_routerProfilesFile = "profiles.dat";
    std::string m_strictConnect;
    FreehandOptions m_options;

    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct NetdbConfig
  {
    std::string m_nodedbDir;

    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct DnsConfig
  {
    FreehandOptions m_options;

    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct LinksConfig
  {
    struct LinkInfo
    {
      std::string interface;
      int addressFamily;
      uint16_t port = -1;
    };
    /// Create a LinkInfo from the given string.
    /// @throws if str does not represent a LinkInfo.
    LinkInfo
    LinkInfoFromINIValues(string_view name, string_view value);

    LinkInfo m_OutboundLink;
    std::vector<LinkInfo> m_InboundLinks;

    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct ConnectConfig
  {
    std::vector<std::string> routers;

    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct ServicesConfig
  {
    std::vector< std::pair< std::string, std::string > > services;
    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct SystemConfig
  {
    std::string pidfile;

    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct ApiConfig
  {
    bool m_enableRPCServer;
    std::string m_rpcBindAddr;

    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct LokidConfig
  {
    bool usingSNSeed;
    bool whitelistRouters;
    fs::path ident_keyfile = "identity.key"; // TODO: derive from [router]:data-dir
    std::string lokidRPCAddr;
    std::string lokidRPCUser;
    std::string lokidRPCPassword;

    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct BootstrapConfig
  {
    std::vector< std::string > routers;
    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct LoggingConfig
  {
    enum class LogType
    {
      Unknown = 0,
      File,
      Json,
      Syslog,
    };
    static LogType LogTypeFromString(const std::string&);

    LogType m_logType;
    LogLevel m_logLevel;
    std::string m_logFile;

    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct SnappConfig
  {
    std::string m_keyfile;
    bool m_reachable;
    int m_hops;
    int m_paths;
    std::vector<std::string> m_snodeBlacklist;
    std::string m_exitNode;
    std::string m_localDNS;
    std::string m_upstreamDNS;
    std::string m_mapAddr;

    void
    defineConfigOptions(Configuration& conf, const ConfigGenParameters& params);
  };

  struct Config
  {
    RouterConfig router;
    NetworkConfig network;
    ConnectConfig connect;
    NetdbConfig netdb;
    DnsConfig dns;
    LinksConfig links;
    ServicesConfig services;
    SystemConfig system;
    ApiConfig api;
    LokidConfig lokid;
    BootstrapConfig bootstrap;
    LoggingConfig logging;

    // Initialize config definition
    void
    initializeConfig(Configuration& conf, const ConfigGenParameters& params);

    // Load a config from the given file
    bool
    Load(const char* fname, bool isRelay, fs::path defaultDataDir);

    std::string
    generateBaseClientConfig(fs::path defaultDataDir);

    std::string
    generateBaseRouterConfig(fs::path defaultDataDir);
  };

  fs::path
  GetDefaultDataDir();

  fs::path
  GetDefaultConfigFilename();

  fs::path
  GetDefaultConfigPath();

  void
  ensureConfig(const fs::path& defaultDataDir,
               const fs::path& confFile,
               bool overwrite,
               bool asRouter);

}  // namespace llarp

#endif
