#ifndef LLARP_CONFIG_HPP
#define LLARP_CONFIG_HPP

#include <crypto/types.hpp>
#include <router_contact.hpp>
#include <util/fs.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace llarp
{
  struct ConfigParser;

  class RouterConfig
  {
   private:
    /// always maintain this many connections to other routers
    size_t m_minConnectedRouters = 2;

    /// hard upperbound limit on the number of router to router connections
    size_t m_maxConnectedRouters = 2000;

    std::string m_netId;
    std::string m_nickname;

    fs::path m_encryptionKeyfile = "encryption.key";

    // path to write our self signed rc to
    fs::path m_ourRcFile = "rc.signed";

    // transient iwp encryption key
    fs::path m_transportKeyfile = "transport.key";

    // long term identity key
    fs::path m_identKeyfile = "identity.key";

    bool m_publicOverride = false;
    struct sockaddr_in m_ip4addr;
    AddressInfo m_addrInfo;

    int m_workerThreads;
    int m_numNethreads;

   public:
    // clang-format off
    size_t minConnectedRouters() const        { return m_minConnectedRouters; }
    size_t maxConnectedRouters() const        { return m_maxConnectedRouters; }
    const fs::path& encryptionKeyfile() const { return m_encryptionKeyfile; }
    const fs::path& ourRcFile() const         { return m_ourRcFile; }
    const fs::path& transportKeyfile() const  { return m_transportKeyfile; }
    const fs::path& identKeyfile() const      { return m_identKeyfile; }
    const std::string& netId() const          { return m_netId; }
    const std::string& nickname() const       { return m_nickname; }
    bool publicOverride() const               { return m_publicOverride; }
    const struct sockaddr_in& ip4addr() const { return m_ip4addr; }
    const AddressInfo& addrInfo() const       { return m_addrInfo; }
    int workerThreads() const                 { return m_workerThreads; }
    int numNethreads() const                  { return m_numNethreads; }

    // clang-format on

    bool
    fromSection(string_view key, string_view val);
  };

  struct NetworkConfig
  {
    absl::optional< bool > enableProfiling;
    std::string routerProfilesFile = "profiles.dat";
    std::string strictConnect;
    std::unordered_multimap< std::string, std::string > netConfig;

    bool
    fromSection(string_view key, string_view val);
  };

  struct NetdbConfig
  {
    std::string nodedb_dir;

    bool
    fromSection(string_view key, string_view val);
  };

  struct DnsConfig
  {
    std::unordered_multimap< std::string, std::string > netConfig;

    bool
    fromSection(string_view key, string_view val);
  };

  struct IwpConfig
  {
    uint16_t m_OutboundPort = 0;

    std::vector< std::tuple< std::string, int, uint16_t > > servers;

    bool
    fromSection(string_view key, string_view val);
  };

  struct ConnectConfig
  {
    std::vector< std::string > routers;

    bool
    fromSection(string_view key, string_view val);
  };

  struct ServicesConfig
  {
    std::vector< std::pair< std::string, std::string > > services;
    bool
    fromSection(string_view key, string_view val);
  };

  struct SystemConfig
  {
    std::string pidfile;

    bool
    fromSection(string_view key, string_view val);
  };

  struct MetricsConfig
  {
    bool disableMetrics    = false;
    bool disableMetricLogs = false;
    fs::path jsonMetricsPath;
    std::string metricTankHost;
    std::map< std::string, std::string > metricTags;

    bool
    fromSection(string_view key, string_view val);
  };

  struct ApiConfig
  {
    bool enableRPCServer    = false;
    std::string rpcBindAddr = "127.0.0.1:1190";

    bool
    fromSection(string_view key, string_view val);
  };

  struct LokidConfig
  {
    bool usingSNSeed         = false;
    bool whitelistRouters    = false;
    fs::path ident_keyfile   = "identity.key";
    std::string lokidRPCAddr = "127.0.0.1:22023";
    std::string lokidRPCUser;
    std::string lokidRPCPassword;

    bool
    fromSection(string_view key, string_view val);
  };

  struct BootstrapConfig
  {
    std::vector< std::string > routers;
    bool
    fromSection(string_view key, string_view val);
  };

  struct LoggingConfig
  {
    bool m_LogJSON  = false;
    FILE* m_LogFile = stdout;

    bool
    fromSection(string_view key, string_view val);
  };

  struct Config
  {
   private:
    bool
    parse(const ConfigParser& parser);

   public:
    RouterConfig router;
    NetworkConfig network;
    ConnectConfig connect;
    NetdbConfig netdb;
    DnsConfig dns;
    IwpConfig iwp_links;
    ServicesConfig services;
    SystemConfig system;
    MetricsConfig metrics;
    ApiConfig api;
    LokidConfig lokid;
    BootstrapConfig bootstrap;
    LoggingConfig logging;

    bool
    Load(const char* fname);

    bool
    LoadFromString(string_view str);
  };

}  // namespace llarp

void
llarp_generic_ensure_config(std::ofstream& f, std::string basepath);

void
llarp_ensure_router_config(std::ofstream& f, std::string basepath);

bool
llarp_ensure_client_config(std::ofstream& f, std::string basepath);

#endif
