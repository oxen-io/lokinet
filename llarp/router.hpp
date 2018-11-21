#ifndef LLARP_ROUTER_HPP
#define LLARP_ROUTER_HPP
#include <llarp/dht.h>
#include <llarp/nodedb.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/path.hpp>
#include <llarp/link_layer.hpp>
#include <llarp/rpc.hpp>

#include <functional>
#include <list>
#include <map>
#include <vector>
#include <unordered_map>

#include <llarp/dht.hpp>
#include <llarp/handlers/tun.hpp>
#include <llarp/link_message.hpp>
#include <llarp/routing/handler.hpp>
#include <llarp/service.hpp>
#include <llarp/establish_job.hpp>
#include <llarp/profiling.hpp>
#include <llarp/exit.hpp>

#include "crypto.hpp"
#include "fs.hpp"
#include "mem.hpp"
#include "str.hpp"

bool
llarp_findOrCreateEncryption(llarp_crypto *crypto, const char *fpath,
                             llarp::SecretKey &encryption);

struct TryConnectJob;

struct llarp_router
{
  bool ready;
  // transient iwp encryption key
  fs::path transport_keyfile = "transport.key";

  // nodes to connect to on startup
  std::map< std::string, fs::path > connect;

  // long term identity key
  fs::path ident_keyfile = "identity.key";

  fs::path encryption_keyfile = "encryption.key";

  // path to write our self signed rc to
  fs::path our_rc_file = "rc.signed";

  // our router contact
  llarp::RouterContact _rc;

  /// should we obey the service node whitelist?
  bool whitelistRouters = false;

  const llarp::RouterContact &
  rc() const
  {
    return _rc;
  }

  // our ipv4 public setting
  bool publicOverride = false;
  struct sockaddr_in ip4addr;
  llarp::AddressInfo addrInfo;

  llarp_ev_loop *netloop;
  llarp_threadpool *tp;
  llarp_logic *logic;
  llarp_crypto crypto;
  llarp::path::PathContext paths;
  llarp::exit::Context exitContext;
  llarp::SecretKey identity;
  llarp::SecretKey encryption;
  llarp_threadpool *disk;
  llarp_dht_context *dht = nullptr;

  llarp_nodedb *nodedb;

  // buffer for serializing link messages
  byte_t linkmsg_buffer[MAX_LINK_MSG_SIZE];

  /// always maintain this many connections to other routers
  size_t minConnectedRouters = 5;
  /// hard upperbound limit on the number of router to router connections
  size_t maxConnectedRouters = 2000;

  size_t minRequiredRouters = 4;

  // should we be sending padded messages every interval?
  bool sendPadding = false;

  uint32_t ticker_job_id = 0;

  llarp::InboundMessageParser inbound_link_msg_parser;
  llarp::routing::InboundMessageParser inbound_routing_msg_parser;

  llarp::service::Context hiddenServiceContext;

  std::string defaultIfAddr = "auto";
  std::string defaultIfName = "auto";

  /// default exit config
  llarp::exit::Context::Config_t exitConf;

  bool
  ExitEnabled() const
  {
    auto itr = exitConf.find("exit");
    if(itr == exitConf.end())
      return false;
    return llarp::IsTrueValue(itr->second.c_str());
  }

  bool
  CreateDefaultHiddenService();

  bool
  ShouldCreateDefaultHiddenService();

  const std::string DefaultRPCBindAddr = "127.0.0.1:1190";
  bool enableRPCServer                 = true;
  std::unique_ptr< llarp::rpc::Server > rpcServer;
  std::string rpcBindAddr = DefaultRPCBindAddr;

  /// lokid caller
  const std::string DefaultLokidRPCAddr = "127.0.0.1:22023";
  std::unique_ptr< llarp::rpc::Caller > rpcCaller;
  std::string lokidRPCAddr = DefaultLokidRPCAddr;

  std::unique_ptr< llarp::ILinkLayer > outboundLink;
  std::vector< std::unique_ptr< llarp::ILinkLayer > > inboundLinks;

  llarp::Profiling routerProfiling;
  std::string routerProfilesFile = "profiles.dat";

  typedef std::queue< std::vector< byte_t > > MessageQueue;

  /// outbound message queue
  std::unordered_map< llarp::RouterID, MessageQueue, llarp::RouterID::Hash >
      outboundMessageQueue;

  /// loki verified routers
  std::unordered_map< llarp::RouterID, llarp::RouterContact,
                      llarp::RouterID::Hash >
      validRouters;

  // pending establishing session with routers
  std::unordered_map< llarp::RouterID, std::unique_ptr< TryConnectJob >,
                      llarp::RouterID::Hash >
      pendingEstablishJobs;

  // sessions to persist -> timestamp to end persist at
  std::unordered_map< llarp::RouterID, llarp_time_t, llarp::RouterID::Hash >
      m_PersistingSessions;

  // lokinet routers from lokid, maps pubkey to when we think it will expire,
  // set to max value right now
  std::unordered_map< llarp::PubKey, llarp_time_t, llarp::PubKey::Hash >
      lokinetRouters;

  // TODO: change me if needed
  const std::string defaultUpstreamResolver = "1.1.1.1:53";
  std::list< std::string > upstreamResolvers;
  std::string resolverBindAddr = "127.0.0.1:53";

  llarp_router();
  ~llarp_router();

  void HandleLinkSessionEstablished(llarp::RouterContact);

  bool
  HandleRecvLinkMessageBuffer(llarp::ILinkSession *from, llarp_buffer_t msg);

  void
  AddInboundLink(std::unique_ptr< llarp::ILinkLayer > &link);

  bool
  InitOutboundLink();

  /// initialize us as a service node
  /// return true on success
  bool
  InitServiceNode();

  void
  Close();

  bool
  LoadHiddenServiceConfig(const char *fname);

  bool
  AddHiddenService(const llarp::service::Config::section_t &config);

  bool
  Ready();

  void
  Run();

  void
  PersistSessionUntil(const llarp::RouterID &remote, llarp_time_t until);

  static void
  ConnectAll(void *user, uint64_t orig, uint64_t left);

  bool
  EnsureIdentity();

  bool
  EnsureEncryptionKey();

  bool
  SaveRC();

  const byte_t *
  pubkey() const
  {
    return llarp::seckey_topublic(identity);
  }

  void
  OnConnectTimeout(const llarp::RouterID &remote);

  bool
  HasPendingConnectJob(const llarp::RouterID &remote);

  void
  try_connect(fs::path rcfile);

  bool
  ReloadConfig(const llarp_config *conf);

  /// send to remote router or queue for sending
  /// returns false on overflow
  /// returns true on successful queue
  /// NOT threadsafe
  /// MUST be called in the logic thread
  bool
  SendToOrQueue(const llarp::RouterID &remote, const llarp::ILinkMessage *msg);

  /// sendto or drop
  void
  SendTo(llarp::RouterID remote, const llarp::ILinkMessage *msg,
         llarp::ILinkLayer *chosen);

  /// manually flush outbound message queue for just 1 router
  void
  FlushOutboundFor(const llarp::RouterID remote,
                   llarp::ILinkLayer *chosen = nullptr);

  /// manually discard all pending messages to remote router
  void
  DiscardOutboundFor(const llarp::RouterID &remote);

  /// try establishing a session to a remote router
  void
  TryEstablishTo(const llarp::RouterID &remote);

  void
  ForEachPeer(
      std::function< void(const llarp::ILinkSession *, bool) > visit) const;

  /// flush outbound message queue
  void
  FlushOutbound();

  /// called by link when a remote session is expunged
  void
  SessionClosed(const llarp::RouterID &remote);

  /// call internal router ticker
  void
  Tick();

  /// get time from event loop
  llarp_time_t
  Now() const
  {
    return llarp_ev_loop_time_now_ms(netloop);
  }

  /// schedule ticker to call i ms from now
  void
  ScheduleTicker(uint64_t i = 1000);

  llarp::ILinkLayer *
  GetLinkWithSessionByPubkey(const llarp::RouterID &remote);

  void
  ConnectToRandomRouters(int N);

  size_t
  NumberOfConnectedRouters() const;

  bool
  GetRandomConnectedRouter(llarp::RouterContact &result) const;

  void
  async_verify_RC(const llarp::RouterContact &rc);

  void
  HandleDHTLookupForSendTo(llarp::RouterID remote,
                           const std::vector< llarp::RouterContact > &results);

  bool
  HasSessionTo(const llarp::RouterID &remote) const;

  void
  HandleDHTLookupForTryEstablishTo(
      llarp::RouterID remote,
      const std::vector< llarp::RouterContact > &results);

  static void
  on_verify_client_rc(llarp_async_verify_rc *context);

  static void
  on_verify_server_rc(llarp_async_verify_rc *context);

  static void
  handle_router_ticker(void *user, uint64_t orig, uint64_t left);

  static void
  HandleAsyncLoadRCForSendTo(llarp_async_load_rc *async);

 private:
  template < typename Config >
  void
  mergeHiddenServiceConfig(const Config &in, Config &out)
  {
    for(const auto &resolver : upstreamResolvers)
      out.push_back({"upstream-dns", resolver});
    out.push_back({"local-dns", resolverBindAddr});

    for(const auto &item : in)
      out.push_back({item.first, item.second});
  }
};

#endif
