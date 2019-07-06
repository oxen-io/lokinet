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

  struct RouterConfig
  {
    /// always maintain this many connections to other routers
    size_t minConnectedRouters = 2;

    /// hard upperbound limit on the number of router to router connections
    size_t maxConnectedRouters = 2000;

    std::string netid;
    std::string nickname;

    fs::path encryption_keyfile = "encryption.key";

    // path to write our self signed rc to
    fs::path our_rc_file = "rc.signed";

    // transient iwp encryption key
    fs::path transport_keyfile = "transport.key";

    // long term identity key
    fs::path ident_keyfile = "identity.key";

    bool publicOverride = false;
    struct sockaddr_in ip4addr;
    AddressInfo addrInfo;

    int workerThreads;
    int num_nethreads;

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
    FILE *m_LogFile = stdout;

    bool
    fromSection(string_view key, string_view val);
  };

  struct Config
  {
   private:
    bool
    parse(const ConfigParser &parser);

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
    Load(const char *fname);

    bool
    LoadFromString(string_view str);
  };

}  // namespace llarp

void
llarp_generic_ensure_config(std::ofstream &f, std::string basepath);

void
llarp_ensure_router_config(std::ofstream &f, std::string basepath);

bool
llarp_ensure_client_config(std::ofstream &f, std::string basepath);

#endif
