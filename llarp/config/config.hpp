#ifndef LLARP_CONFIG_HPP
#define LLARP_CONFIG_HPP

#include <crypto/types.hpp>
#include <router_contact.hpp>
#include <util/fs.hpp>
#include <util/str.hpp>

#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>


struct llarp_config;
namespace llarp
{
  struct ConfigParser;

  inline const char*
  lokinetEnv(string_view suffix);

  std::string
  fromEnv(string_view val, string_view envNameSuffix);
  int
  fromEnv(const int& val, string_view envNameSuffix);
  uint16_t
  fromEnv(const uint16_t& val, string_view envNameSuffix);
  size_t
  fromEnv(const size_t& val, string_view envNameSuffix);
  nonstd::optional< bool >
  fromEnv(const nonstd::optional< bool >& val, string_view envNameSuffix);

  class RouterConfig
  {
   private:
    /// always maintain this many connections to other routers
    size_t m_minConnectedRouters = 2;

    /// hard upperbound limit on the number of router to router connections
    size_t m_maxConnectedRouters = 5;

    std::string m_netId;
    std::string m_nickname;

    std::string m_encryptionKeyfile = "encryption.key";

    // path to write our self signed rc to
    std::string m_ourRcFile = "rc.signed";

    // transient iwp encryption key
    std::string m_transportKeyfile = "transport.key";

    // long term identity key
    std::string m_identKeyfile = "identity.key";

    nonstd::optional< bool > m_blockBogons;

    bool m_publicOverride = false;
    struct sockaddr_in m_ip4addr;
    AddressInfo m_addrInfo;

    int m_workerThreads = 1;
    int m_numNetThreads = 1;

    size_t m_JobQueueSize = size_t{1024 * 8};

    std::string m_DefaultLinkProto = "iwp";

   public:
    // clang-format off
    size_t jobQueueSize() const                { return fromEnv(m_JobQueueSize, "JOB_QUEUE_SIZE"); }
    size_t minConnectedRouters() const         { return fromEnv(m_minConnectedRouters, "MIN_CONNECTED_ROUTERS"); }
    size_t maxConnectedRouters() const         { return fromEnv(m_maxConnectedRouters, "MAX_CONNECTED_ROUTERS"); }
    std::string encryptionKeyfile() const      { return fromEnv(m_encryptionKeyfile, "ENCRYPTION_KEYFILE"); }
    std::string ourRcFile() const              { return fromEnv(m_ourRcFile, "OUR_RC_FILE"); }
    std::string transportKeyfile() const       { return fromEnv(m_transportKeyfile, "TRANSPORT_KEYFILE"); }
    std::string identKeyfile() const           { return fromEnv(m_identKeyfile, "IDENT_KEYFILE"); }
    std::string netId() const                  { return fromEnv(m_netId, "NETID"); }
    std::string nickname() const               { return fromEnv(m_nickname, "NICKNAME"); }
    bool publicOverride() const                { return fromEnv(m_publicOverride, "PUBLIC_OVERRIDE"); }
    const struct sockaddr_in& ip4addr() const  { return m_ip4addr; }
    const AddressInfo& addrInfo() const        { return m_addrInfo; }
    int workerThreads() const                  { return fromEnv(m_workerThreads, "WORKER_THREADS"); }
    int numNetThreads() const                  { return fromEnv(m_numNetThreads, "NUM_NET_THREADS"); }
    std::string defaultLinkProto() const       { return fromEnv(m_DefaultLinkProto, "LINK_PROTO"); }
    nonstd::optional< bool > blockBogons() const { return fromEnv(m_blockBogons, "BLOCK_BOGONS"); }
    // clang-format on

    void
    fromSection(string_view key, string_view val);
  };

  class NetworkConfig
  {
   public:
    using NetConfig = std::unordered_multimap< std::string, std::string >;

   private:
    nonstd::optional< bool > m_enableProfiling;
    std::string m_routerProfilesFile = "profiles.dat";
    std::string m_strictConnect;
    NetConfig m_netConfig;

   public:
    // clang-format off
    nonstd::optional< bool > enableProfiling() const { return fromEnv(m_enableProfiling, "ENABLE_PROFILING"); }
    std::string routerProfilesFile() const         { return fromEnv(m_routerProfilesFile, "ROUTER_PROFILES_FILE"); }
    std::string strictConnect() const              { return fromEnv(m_strictConnect, "STRICT_CONNECT"); }
    const NetConfig& netConfig() const             { return m_netConfig; }
    // clang-format on

    void
    fromSection(string_view key, string_view val);
  };

  class NetdbConfig
  {
   private:
    std::string m_nodedbDir;

   public:
    // clang-format off
    std::string nodedbDir() const { return fromEnv(m_nodedbDir, "NODEDB_DIR"); }
    // clang-format on

    void
    fromSection(string_view key, string_view val);
  };

  struct DnsConfig
  {
    std::unordered_multimap< std::string, std::string > netConfig;

    void
    fromSection(string_view key, string_view val);
  };

  class LinksConfig
  {
   public:
    static constexpr int Interface     = 0;
    static constexpr int AddressFamily = 1;
    static constexpr int Port          = 2;
    static constexpr int Options       = 3;

    using ServerOptions = std::unordered_set< std::string >;
    using LinkInfo = std::tuple< std::string, int, uint16_t, ServerOptions >;
    using Links    = std::vector< LinkInfo >;

   private:
    LinkInfo m_OutboundLink;
    Links m_InboundLinks;

   public:
    // clang-format off
    const LinkInfo& outboundLink() const  { return m_OutboundLink; }

    const Links& inboundLinks() const { return m_InboundLinks; }
    // clang-format on

    void
    fromSection(string_view key, string_view val);
  };

  struct ConnectConfig
  {
    std::vector< std::string > routers;

    void
    fromSection(string_view key, string_view val);
  };

  struct ServicesConfig
  {
    std::vector< std::pair< std::string, std::string > > services;
    void
    fromSection(string_view key, string_view val);
  };

  struct SystemConfig
  {
    std::string pidfile;

    void
    fromSection(string_view key, string_view val);
  };

  class ApiConfig
  {
   private:
    bool m_enableRPCServer    = false;
    std::string m_rpcBindAddr = "127.0.0.1:1190";

   public:
    // clang-format off
    bool enableRPCServer() const    { return fromEnv(m_enableRPCServer, "ENABLE_RPC_SERVER"); }
    std::string rpcBindAddr() const { return fromEnv(m_rpcBindAddr, "RPC_BIND_ADDR"); }
    // clang-format on

    void
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

    void
    fromSection(string_view key, string_view val);
  };

  struct BootstrapConfig
  {
    std::vector< std::string > routers;
    void
    fromSection(string_view key, string_view val);
  };

  struct LoggingConfig
  {
    bool m_LogJSON  = false;
    FILE* m_LogFile = stdout;

    void
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
    LinksConfig links;
    ServicesConfig services;
    SystemConfig system;
    ApiConfig api;
    LokidConfig lokid;
    BootstrapConfig bootstrap;
    LoggingConfig logging;

    bool
    Load(const char* fname);

    bool
    LoadFromStr(string_view str);

    llarp_config *
    Copy() const;
  };

  fs::path
  GetDefaultConfigDir();

  fs::path
  GetDefaultConfigPath();

}  // namespace llarp

void
llarp_generic_ensure_config(std::ofstream& f, std::string basepath,
                            bool isRouter);

void
llarp_ensure_router_config(std::ofstream& f, std::string basepath);

bool
llarp_ensure_client_config(std::ofstream& f, std::string basepath);

#endif
