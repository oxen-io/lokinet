#ifndef LLARP_CONFIG_HPP
#define LLARP_CONFIG_HPP

#include <chrono>
#include <crypto/types.hpp>
#include <router_contact.hpp>
#include <util/fs.hpp>
#include <util/str.hpp>
#include <config/ini.hpp>
#include <config/definition.hpp>
#include <constants/files.hpp>
#include <net/ip_address.hpp>
#include <net/net_int.hpp>
#include <service/address.hpp>

#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>

struct llarp_config;

namespace llarp
{
  using SectionValues_t = llarp::ConfigParser::SectionValues_t;
  using Config_impl_t = llarp::ConfigParser::Config_impl_t;

  // TODO: don't use these maps. they're sloppy and difficult to follow
  /// Small struct to gather all parameters needed for config generation to reduce the number of
  /// parameters that need to be passed around.
  struct ConfigGenParameters
  {
    bool isRelay = false;
    fs::path defaultDataDir;
  };

  struct RouterConfig
  {
    size_t m_minConnectedRouters = 0;
    size_t m_maxConnectedRouters = 0;

    std::string m_netId;
    std::string m_nickname;

    fs::path m_dataDir;

    bool m_blockBogons = false;

    IpAddress m_publicAddress;

    int m_workerThreads = -1;
    int m_numNetThreads = -1;

    size_t m_JobQueueSize = 0;

    std::string m_routerContactFile;
    std::string m_encryptionKeyFile;
    std::string m_identityKeyFile;
    std::string m_transportKeyFile;

    void
    defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params);
  };

  struct NetworkConfig
  {
    std::optional<bool> m_enableProfiling;
    std::string m_routerProfilesFile;
    std::string m_strictConnect;
    std::string m_ifname;
    std::string m_ifaddr;

    std::optional<fs::path> m_keyfile;
    std::string m_endpointType;
    bool m_reachable = false;
    std::optional<int> m_Hops;
    std::optional<int> m_Paths;
    bool m_AllowExit = false;
    std::set<RouterID> m_snodeBlacklist;
    std::optional<service::Address> m_exitNode;
    std::unordered_map<huint128_t, service::Address> m_mapAddrs;

    // TODO:
    // on-up
    // on-down
    // on-ready

    void
    defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params);
  };

  struct DnsConfig
  {
    IpAddress m_bind;
    std::vector<IpAddress> m_upstreamDNS;

    void
    defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params);
  };

  struct LinksConfig
  {
    struct LinkInfo
    {
      std::string interface;
      int addressFamily = -1;
      uint16_t port = -1;
    };
    /// Create a LinkInfo from the given string.
    /// @throws if str does not represent a LinkInfo.
    LinkInfo
    LinkInfoFromINIValues(std::string_view name, std::string_view value);

    LinkInfo m_OutboundLink;
    std::vector<LinkInfo> m_InboundLinks;

    void
    defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params);
  };

  struct ConnectConfig
  {
    std::vector<fs::path> routers;

    void
    defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params);
  };

  struct ApiConfig
  {
    bool m_enableRPCServer = false;
    std::string m_rpcBindAddr;

    void
    defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params);
  };

  struct LokidConfig
  {
    bool usingSNSeed = false;
    bool whitelistRouters = false;
    fs::path ident_keyfile;
    std::string lokidRPCAddr;
    std::string lokidRPCUser;
    std::string lokidRPCPassword;

    void
    defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params);
  };

  struct BootstrapConfig
  {
    std::vector<fs::path> routers;
    /// for unit tests
    bool skipBootstrap = false;
    void
    defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params);
  };

  struct LoggingConfig
  {
    LogType m_logType = LogType::Unknown;
    LogLevel m_logLevel = eLogNone;
    std::string m_logFile;

    void
    defineConfigOptions(ConfigDefinition& conf, const ConfigGenParameters& params);
  };

  struct Config
  {
    RouterConfig router;
    NetworkConfig network;
    ConnectConfig connect;
    DnsConfig dns;
    LinksConfig links;
    ApiConfig api;
    LokidConfig lokid;
    BootstrapConfig bootstrap;
    LoggingConfig logging;

    // Initialize config definition
    void
    initializeConfig(ConfigDefinition& conf, const ConfigGenParameters& params);

    /// Insert config entries for backwards-compatibility (e.g. so that the config system will
    /// tolerate old values that are no longer accepted)
    ///
    /// @param conf is the config to modify
    void
    addBackwardsCompatibleConfigOptions(ConfigDefinition& conf);

    // Load a config from the given file
    bool
    Load(const fs::path fname, bool isRelay, fs::path defaultDataDir);

    /// Load (initialize) a default config.
    ///
    /// This delegates to the ConfigDefinition to generate a default config,
    /// as though an empty config were specified.
    ///
    /// If using Config without the intention of loading from file (or string), this is necessary
    /// in order to obtain sane defaults.
    ///
    /// @param isRelay determines whether the config will reflect that of a relay or client
    /// @param dataDir is a path representing a directory to be used as the data dir
    /// @return true on success, false otherwise
    bool
    LoadDefault(bool isRelay, fs::path dataDir);

    std::string
    generateBaseClientConfig(fs::path defaultDataDir);

    std::string
    generateBaseRouterConfig(fs::path defaultDataDir);

    llarp_config*
    Copy() const;
  };

  void
  ensureConfig(
      const fs::path& defaultDataDir, const fs::path& confFile, bool overwrite, bool asRouter);

}  // namespace llarp

struct llarp_config
{
  llarp::Config impl;
  llarp_config() = default;

  explicit llarp_config(const llarp_config* other);
};

#endif
