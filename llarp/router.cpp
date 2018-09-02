#include "router.hpp"
#include <llarp/proto.h>
#include <llarp/iwp.hpp>
#include <llarp/link_message.hpp>
#include <llarp/link/curvecp.hpp>

#include "buffer.hpp"
#include "encode.hpp"
#include "llarp/net.hpp"
#include "logger.hpp"
#include "str.hpp"

#include <fstream>

namespace llarp
{
  void
  router_iter_config(llarp_config_iterator *iter, const char *section,
                     const char *key, const char *val);

  struct async_verify_context
  {
    llarp_router *router;
    llarp::OutboundLinkEstablishJob *establish_job;
  };

}  // namespace llarp

struct TryConnectJob : public llarp::OutboundLinkEstablishJob
{
  llarp::ILinkLayer *link;
  llarp_router *router;
  uint16_t triesLeft;
  TryConnectJob(const llarp::RouterContact &remote, llarp::ILinkLayer *l,
                uint16_t tries, llarp_router *r)
      : OutboundLinkEstablishJob(remote), link(l), router(r), triesLeft(tries)
  {
  }

  void
  Failed()
  {
    link->CloseSessionTo(rc.pubkey);
  }

  void
  Success()
  {
    router->FlushOutboundFor(rc.pubkey, link);
    router->pendingEstablishJobs.erase(rc.pubkey);
    // we are gone
  }

  void
  AttemptTimedout()
  {
    --triesLeft;
    if(!ShouldRetry())
    {
      router->pendingEstablishJobs.erase(rc.pubkey);
      // we are gone after this
      return;
    }
    Attempt();
  }

  void
  Attempt()
  {
    link->TryEstablishTo(rc);
  }

  bool
  ShouldRetry() const
  {
    return triesLeft > 0;
  }
};

static void
on_try_connecting(void *u)
{
  llarp::OutboundLinkEstablishJob *j =
      static_cast< llarp::OutboundLinkEstablishJob * >(u);
  j->Attempt();
}

bool
llarp_router_try_connect(struct llarp_router *router,
                         const llarp::RouterContact &remote,
                         uint16_t numretries)
{
  // do we already have a pending job for this remote?
  if(router->HasPendingConnectJob(remote.pubkey))
  {
    llarp::LogDebug("We have pending connect jobs to ", remote.pubkey);
    return false;
  }

  auto link = router->outboundLink.get();
  auto itr  = router->pendingEstablishJobs.insert(std::make_pair(
      remote.pubkey, new TryConnectJob(remote, link, numretries, router)));
  llarp::OutboundLinkEstablishJob *job = itr.first->second;
  // try establishing async
  llarp_logic_queue_job(router->logic, {job, on_try_connecting});
  return true;
}

llarp_router::llarp_router()
    : ready(false)
    , paths(this)
    , dht(llarp_dht_context_new(this))
    , inbound_link_msg_parser(this)
    , hiddenServiceContext(this)

{
  // set rational defaults
  this->ip4addr.sin_family = AF_INET;
  this->ip4addr.sin_port   = htons(1090);
}

llarp_router::~llarp_router()
{
  llarp_dht_context_free(dht);
}

bool
llarp_router::HandleRecvLinkMessageBuffer(llarp::ILinkSession *session,
                                          llarp_buffer_t buf)
{
  return inbound_link_msg_parser.ProcessFrom(session, buf);
}

void
llarp_router::PersistSessionUntil(const llarp::RouterID &remote,
                                  llarp_time_t until)
{
  llarp::LogDebug("persist session to ", remote, " until ", until);
  if(m_PersistingSessions.find(remote) == m_PersistingSessions.end())
    m_PersistingSessions[remote] = until;
  else
  {
    if(m_PersistingSessions[remote] < until)
      m_PersistingSessions[remote] = until;
  }
}

bool
llarp_router::SendToOrQueue(const llarp::RouterID &remote,
                            const llarp::ILinkMessage *m)
{
  std::unique_ptr< const llarp::ILinkMessage > msg =
      std::unique_ptr< const llarp::ILinkMessage >(m);
  llarp::ILinkLayer *chosen = nullptr;

  if(inboundLinks.size() == 0)
    chosen = outboundLink.get();
  else
    chosen = inboundLinks.front().get();

  if(chosen->HasSessionTo(remote))
  {
    SendTo(remote, msg, chosen);
    return true;
  }
  // this will create an entry in the obmq if it's not already there
  auto itr = outboundMessageQueue.find(remote);
  if(itr == outboundMessageQueue.end())
  {
    outboundMessageQueue.insert(std::make_pair(remote, MessageQueue()));
  }
  // encode
  llarp_buffer_t buf =
      llarp::StackBuffer< decltype(linkmsg_buffer) >(linkmsg_buffer);
  if(!msg->BEncode(&buf))
    return false;
  // queue buffer
  auto &q = outboundMessageQueue[remote];
  buf.sz  = buf.cur - buf.base;
  q.emplace(buf.sz);
  memcpy(q.back().data(), buf.base, buf.sz);

  // we don't have an open session to that router right now
  if(llarp_nodedb_get_rc(nodedb, remote, rc))
  {
    // try connecting directly as the rc is loaded from disk
    llarp_router_try_connect(this, rc, 10);
    return true;
  }

  // we don't have the RC locally so do a dht lookup
  dht->impl.LookupRouter(remote,
                         std::bind(&llarp_router::HandleDHTLookupForSendTo,
                                   this, remote, std::placeholders::_1));
  return true;
}

void
llarp_router::HandleDHTLookupForSendTo(
    llarp::RouterID remote, const std::vector< llarp::RouterContact > &results)
{
  if(results.size())
  {
    llarp_nodedb_put_rc(nodedb, results[0]);
    llarp_router_try_connect(this, results[0], 10);
  }
  else
  {
    DiscardOutboundFor(remote);
  }
}

void
llarp_router::try_connect(fs::path rcfile)
{
  llarp::RouterContact remote;
  if(!remote.Read(rcfile.string().c_str()))
  {
    llarp::LogError("failure to decode or verify of remote RC");
    return;
  }
  if(remote.VerifySignature(&crypto))
  {
    llarp::LogDebug("verified signature");
    // store into filesystem
    if(!llarp_nodedb_put_rc(nodedb, remote))
    {
      llarp::LogWarn("failed to store");
    }
    if(!llarp_router_try_connect(this, remote, 10))
    {
      // or error?
      llarp::LogWarn("session already made");
    }
  }
  else
    llarp::LogError("failed to verify signature of RC", rcfile);
}

bool
llarp_router::EnsureIdentity()
{
  if(!EnsureEncryptionKey())
    return false;
  return llarp_findOrCreateIdentity(&crypto, ident_keyfile.string().c_str(),
                                    identity);
}

bool
llarp_router::EnsureEncryptionKey()
{
  return llarp_findOrCreateEncryption(
      &crypto, encryption_keyfile.string().c_str(), encryption);
}

void
llarp_router::AddInboundLink(std::unique_ptr< llarp::ILinkLayer > &link)
{
  inboundLinks.push_back(std::move(link));
}

bool
llarp_router::Ready()
{
  return outboundLink != nullptr;
}

bool
llarp_router::SaveRC()
{
  llarp::LogDebug("verify RC signature");
  if(!rc.VerifySignature(&crypto))
  {
    rc.Dump< MAX_RC_SIZE >();
    llarp::LogError("RC has bad signature not saving");
    return false;
  }
  return rc.Write(our_rc_file.string().c_str());
}

void
llarp_router::Close()
{
  llarp::LogInfo("Closing ", inboundLinks.size(), " server bindings");
  for(const auto &link : inboundLinks)
  {
    link->Stop();
  }
  inboundLinks.clear();

  llarp::LogInfo("Closing LokiNetwork client");
  if(outboundLink)
  {
    outboundLink->Stop();
    outboundLink.reset(nullptr);
  }
}

void
llarp_router::on_verify_client_rc(llarp_async_verify_rc *job)
{
  llarp::async_verify_context *ctx =
      static_cast< llarp::async_verify_context * >(job->user);
  ctx->router->pendingEstablishJobs.erase(job->rc.pubkey);
  delete ctx;
}

void
llarp_router::on_verify_server_rc(llarp_async_verify_rc *job)
{
  llarp::async_verify_context *ctx =
      static_cast< llarp::async_verify_context * >(job->user);
  auto router = ctx->router;
  llarp::PubKey pk(job->rc.pubkey);
  if(!job->valid)
  {
    llarp::LogWarn("invalid server RC");
    if(ctx->establish_job)
    {
      // was an outbound attempt
      ctx->establish_job->Failed();
    }
    router->DiscardOutboundFor(pk);
    return;
  }
  // we're valid, which means it's already been committed to the nodedb

  llarp::LogDebug("rc verified and saved to nodedb");

  // refresh valid routers RC value if it's there
  router->validRouters[pk] = job->rc;

  // track valid router in dht
  router->dht->impl.nodes->PutNode(job->rc);

  // this was an outbound establish job
  if(ctx->establish_job)
  {
    ctx->establish_job->Success();
  }
  else  // this was an inbound session
    router->FlushOutboundFor(pk, router->GetLinkWithSessionByPubkey(pk));
}

void
llarp_router::handle_router_ticker(void *user, uint64_t orig, uint64_t left)
{
  if(left)
    return;
  llarp_router *self  = static_cast< llarp_router * >(user);
  self->ticker_job_id = 0;
  self->Tick();
  self->ScheduleTicker(orig);
}

void
llarp_router::TryEstablishTo(const llarp::RouterID &remote)
{
  llarp::RouterContact rc;
  if(llarp_nodedb_get_rc(nodedb, remote, rc))
  {
    // try connecting async
    llarp_router_try_connect(this, rc, 5);
  }
  else
  {
    // dht lookup as we don't know it
    dht->impl.LookupRouter(
        remote,
        std::bind(&llarp_router::HandleDHTLookupForTryEstablishTo, this,
                  std::placeholders::_1));
  }
}

void
llarp_router::HandleDHTLookupForTryEstablishTo(
    const std::vector< llarp::RouterContact > &results)
{
  for(const auto &result : results)
    async_verify_RC(result, false);
}

size_t
llarp_router::NumberOfConnectedRouters() const
{
  return validRouters.size();
}

void
llarp_router::Tick()
{
  // llarp::LogDebug("tick router");
  auto now = llarp_time_now_ms();
  paths.ExpirePaths();
  {
    auto itr = m_PersistingSessions.begin();
    while(itr != m_PersistingSessions.end())
    {
      auto link = GetLinkWithSessionByPubkey(itr->first);
      if(now > itr->second)
      {
        // persisting ended
        if(link)
          link->CloseSessionTo(itr->first);
        itr = m_PersistingSessions.erase(itr);
      }
      else
      {
        if(link)
        {
          llarp::LogDebug("keepalive to ", itr->first);
          link->KeepAliveSessionTo(itr->first);
        }
        else
        {
          llarp::LogDebug("establish to ", itr->first);
          TryEstablishTo(itr->first);
        }
        ++itr;
      }
    }
  }

  if(inboundLinks.size() == 0)
  {
    auto N = llarp_nodedb_num_loaded(nodedb);
    if(N > 3)
    {
      paths.BuildPaths();
    }
    else
    {
      llarp::LogInfo(
          "We need more than 3 service nodes to build paths but we have ", N);
    }
    hiddenServiceContext.Tick();
  }
  paths.TickPaths();
}

void
llarp_router::SendTo(llarp::RouterID remote,
                     std::unique_ptr< const llarp::ILinkMessage > &msg,
                     llarp::ILinkLayer *selected)
{
  llarp_buffer_t buf =
      llarp::StackBuffer< decltype(linkmsg_buffer) >(linkmsg_buffer);

  if(!msg->BEncode(&buf))
  {
    llarp::LogWarn("failed to encode outbound message, buffer size left: ",
                   llarp_buffer_size_left(buf));
    return;
  }
  // set size of message
  buf.sz  = buf.cur - buf.base;
  buf.cur = buf.base;
  if(selected)
  {
    selected->SendTo(remote, buf);
    return;
  }
  bool sent = outboundLink->SendTo(remote, buf);
  if(!sent)
  {
    for(const auto &link : inboundLinks)
    {
      if(!sent)
      {
        sent = link->SendTo(remote, buf);
      }
    }
  }
  if(!sent)
    llarp::LogWarn("message to ", remote, " was dropped");
}

void
llarp_router::ScheduleTicker(uint64_t ms)
{
  ticker_job_id =
      llarp_logic_call_later(logic, {ms, this, &handle_router_ticker});
}

void
llarp_router::SessionClosed(const llarp::RouterID &remote)
{
  // remove from valid routers and dht if it's a valid router
  auto itr = validRouters.find(remote);
  if(itr == validRouters.end())
    return;
  __llarp_dht_remove_peer(dht, remote);
  validRouters.erase(itr);
}

llarp::ILinkLayer *
llarp_router::GetLinkWithSessionByPubkey(const llarp::RouterID &pubkey)
{
  if(outboundLink->HasSessionTo(pubkey))
    return outboundLink.get();
  for(const auto &link : inboundLinks)
  {
    if(link->HasSessionTo(pubkey))
      return link.get();
  }
  return nullptr;
}

void
llarp_router::FlushOutboundFor(const llarp::RouterID &remote,
                               llarp::ILinkLayer *chosen)
{
  llarp::LogDebug("Flush outbound for ", remote);
  auto itr = outboundMessageQueue.find(remote);
  if(itr == outboundMessageQueue.end())
  {
    return;
  }
  if(!chosen)
  {
    DiscardOutboundFor(remote);
    return;
  }
  while(itr->second.size())
  {
    auto buf = llarp::ConstBuffer(itr->second.front());
    if(!chosen->SendTo(remote, buf))
      llarp::LogWarn("failed to send outboud message to ", remote, " via ",
                     chosen->Name());

    itr->second.pop();
  }
}

void
llarp_router::DiscardOutboundFor(const llarp::RouterID &remote)
{
  outboundMessageQueue.erase(remote);
}

bool
llarp_router::GetRandomConnectedRouter(llarp::RouterContact &result) const
{
  auto sz = validRouters.size();
  if(sz)
  {
    auto itr = validRouters.begin();
    if(sz > 1)
      std::advance(itr, llarp_randint() % sz);
    result = itr->second;
    return true;
  }
  return false;
}

void
llarp_router::async_verify_RC(const llarp::RouterContact &rc,
                              bool isExpectingClient,
                              llarp::OutboundLinkEstablishJob *establish_job)
{
  llarp_async_verify_rc *job       = new llarp_async_verify_rc();
  llarp::async_verify_context *ctx = new llarp::async_verify_context();
  ctx->router                      = this;
  ctx->establish_job               = establish_job;
  job->user                        = ctx;
  job->rc                          = rc;
  job->valid                       = false;
  job->hook                        = nullptr;

  job->nodedb = nodedb;
  job->logic  = logic;
  // job->crypto = &crypto; // we already have this
  job->cryptoworker = tp;
  job->diskworker   = disk;

  if(isExpectingClient)
    job->hook = &llarp_router::on_verify_client_rc;
  else
    job->hook = &llarp_router::on_verify_server_rc;
  llarp_nodedb_async_verify(job);
}

void
llarp_router::Run()
{
  // zero out router contact
  sockaddr *dest = (sockaddr *)&this->ip4addr;
  llarp::Addr publicAddr(*dest);
  if(this->publicOverride)
  {
    if(publicAddr)
    {
      llarp::LogInfo("public address:port ", publicAddr);
    }
  }

  llarp::LogInfo("You have ", inboundLinks.size(), " inbound links");
  for(const auto &link : inboundLinks)
  {
    llarp::AddressInfo addr;
    if(!link->GetOurAddressInfo(addr))
      continue;
    llarp::Addr a(addr);
    if(this->publicOverride && a.sameAddr(publicAddr))
    {
      llarp::LogInfo("Found adapter for public address");
    }
    if(!a.isPrivate())
    {
      llarp::LogInfo("Loading Addr: ", a, " into our RC");
      rc.addrs.push_back(addr);
    }
  };
  if(this->publicOverride)
  {
    llarp::ILinkLayer *link = nullptr;
    // llarp::LogWarn("Need to load our public IP into RC!");
    if(inboundLinks.size() == 1)
    {
      link = inboundLinks.front().get();
    }
    else
    {
      if(!inboundLinks.size())
      {
        llarp::LogError("No inbound links found, aborting");
        return;
      }
      link = inboundLinks.front().get();
    }
    if(link->GetOurAddressInfo(this->addrInfo))
    {
      // override ip and port
      this->addrInfo.ip   = *publicAddr.addr6();
      this->addrInfo.port = publicAddr.port();
      llarp::LogInfo("Loaded our public ", publicAddr, " override into RC!");
      // we need the link to set the pubkey
      rc.addrs.push_back(this->addrInfo);
    }
  }
  // set public encryption key
  rc.enckey = llarp::seckey_topublic(encryption);
  llarp::LogInfo("Your Encryption pubkey ", rc.enckey);
  // set public signing key
  rc.pubkey = llarp::seckey_topublic(identity);
  llarp::LogInfo("Your Identity pubkey ", rc.pubkey);

  llarp::LogInfo("Signing rc...");
  if(!rc.Sign(&crypto, identity))
  {
    llarp::LogError("failed to sign rc");
    return;
  }

  if(!SaveRC())
  {
    return;
  }

  llarp::LogDebug("starting outbound link");
  if(!outboundLink->Start(logic))
  {
    llarp::LogWarn("outbound link failed to start");
  }

  int IBLinksStarted = 0;

  // start links
  for(const auto &link : inboundLinks)
  {
    if(link->Start(logic))
    {
      llarp::LogDebug("Link ", link->Name(), " started");
      IBLinksStarted++;
    }
    else
      llarp::LogWarn("Link ", link->Name(), " failed to start");
  }

  if(IBLinksStarted > 0)
  {
    // initialize as service node
    InitServiceNode();
    // immediate connect all for service node
    uint64_t delay = llarp_randint() % 100;
    llarp_logic_call_later(logic, {delay, this, &ConnectAll});
  }
  else
  {
    // delayed connect all for clients
    uint64_t delay = ((llarp_randint() % 10) * 500) + 500;
    llarp_logic_call_later(logic, {delay, this, &ConnectAll});
  }

  llarp::PubKey ourPubkey = pubkey();
  llarp::LogInfo("starting dht context as ", ourPubkey);
  llarp_dht_context_start(dht, ourPubkey);

  ScheduleTicker(1000);
}

void
llarp_router::InitServiceNode()
{
  llarp::LogInfo("accepting transit traffic");
  paths.AllowTransit();
  llarp_dht_allow_transit(dht);
}

void
llarp_router::ConnectAll(void *user, uint64_t orig, uint64_t left)
{
  if(left)
    return;
  llarp_router *self = static_cast< llarp_router * >(user);
  for(const auto &itr : self->connect)
  {
    llarp::LogInfo("connecting to node ", itr.first);
    self->try_connect(itr.second);
  }
}
bool
llarp_router::InitOutboundLink()
{
  if(outboundLink)
    return true;

  auto link = llarp::curvecp::NewServer(this);

  auto afs = {AF_INET, AF_INET6};

  for(auto af : afs)
  {
    if(link->Configure(netloop, "*", af, 0))
    {
      outboundLink = std::move(link);
      llarp::LogInfo("outbound link ready");
      return true;
    }
  }
  return false;
}

bool
llarp_router::HasPendingConnectJob(const llarp::RouterID &remote)
{
  return pendingEstablishJobs.find(remote) != pendingEstablishJobs.end();
}

struct llarp_router *
llarp_init_router(struct llarp_threadpool *tp, struct llarp_ev_loop *netloop,
                  struct llarp_logic *logic)
{
  llarp_router *router = new llarp_router();
  if(router)
  {
    router->netloop = netloop;
    router->tp      = tp;
    router->logic   = logic;
// TODO: make disk io threadpool count configurable
#ifdef TESTNET
    router->disk = tp;
#else
    router->disk = llarp_init_threadpool(1, "llarp-diskio");
#endif
    llarp_crypto_init(&router->crypto);
  }
  return router;
}

bool
llarp_configure_router(struct llarp_router *router, struct llarp_config *conf)
{
  llarp_config_iterator iter;
  iter.user  = router;
  iter.visit = llarp::router_iter_config;
  llarp_config_iter(conf, &iter);
  if(!router->InitOutboundLink())
    return false;
  if(!router->Ready())
  {
    return false;
  }
  return router->EnsureIdentity();
}

void
llarp_run_router(struct llarp_router *router, struct llarp_nodedb *nodedb)
{
  router->nodedb = nodedb;
  router->Run();
}

void
llarp_stop_router(struct llarp_router *router)
{
  if(router)
    router->Close();
}

void
llarp_free_router(struct llarp_router **router)
{
  if(*router)
  {
    delete *router;
  }
  *router = nullptr;
}

bool
llarp_findOrCreateIdentity(llarp_crypto *crypto, const char *fpath,
                           byte_t *secretkey)
{
  llarp::LogDebug("find or create ", fpath);
  fs::path path(fpath);
  std::error_code ec;
  if(!fs::exists(path, ec))
  {
    llarp::LogInfo("generating new identity key");
    crypto->identity_keygen(secretkey);
    std::ofstream f(path.string(), std::ios::binary);
    if(f.is_open())
    {
      f.write((char *)secretkey, SECKEYSIZE);
    }
  }
  std::ifstream f(path.string(), std::ios::binary);
  if(f.is_open())
  {
    f.read((char *)secretkey, SECKEYSIZE);
    return true;
  }
  llarp::LogInfo("failed to get identity key");
  return false;
}

// C++ ...
bool
llarp_findOrCreateEncryption(llarp_crypto *crypto, const char *fpath,
                             llarp::SecretKey &encryption)
{
  llarp::LogDebug("find or create ", fpath);
  fs::path path(fpath);
  std::error_code ec;
  if(!fs::exists(path, ec))
  {
    llarp::LogInfo("generating new encryption key");
    crypto->encryption_keygen(encryption);
    std::ofstream f(path.string(), std::ios::binary);
    if(f.is_open())
    {
      f.write((char *)encryption.data(), SECKEYSIZE);
    }
  }

  std::ifstream f(path.string(), std::ios::binary);
  if(f.is_open())
  {
    f.read((char *)encryption.data(), SECKEYSIZE);
    return true;
  }
  llarp::LogInfo("failed to get encryption key");
  return false;
}

bool
llarp_router::LoadHiddenServiceConfig(const char *fname)
{
  llarp::LogDebug("opening hidden service config ", fname);
  llarp::service::Config conf;
  if(!conf.Load(fname))
    return false;
  for(const auto &config : conf.services)
  {
    if(!hiddenServiceContext.AddEndpoint(config))
      return false;
  }
  return true;
}

namespace llarp
{
  void
  router_iter_config(llarp_config_iterator *iter, const char *section,
                     const char *key, const char *val)
  {
    llarp_router *self = static_cast< llarp_router * >(iter->user);

    int af;
    uint16_t proto;
    if(StrEq(val, "eth"))
    {
#ifdef AF_LINK
      af = AF_LINK;
#endif
#ifdef AF_PACKET
      af = AF_PACKET;
#endif
      proto = LLARP_ETH_PROTO;
    }
    else
    {
      // try IPv4 first
      af    = AF_INET;
      proto = std::atoi(val);
    }

    if(StrEq(section, "bind"))
    {
      if(!StrEq(key, "*"))
      {
        auto server = llarp::curvecp::NewServer(self);
        if(server->Configure(self->netloop, key, af, proto))
        {
          self->AddInboundLink(server);
          return;
        }
        if(af == AF_INET6)
        {
          // we failed to configure IPv6
          // try IPv4
          llarp::LogInfo("link ", key,
                         " failed to configure IPv6, trying IPv4");
          af = AF_INET;
          if(server->Configure(self->netloop, key, af, proto))
          {
            self->AddInboundLink(server);
            return;
          }
        }
        llarp::LogError("Failed to set up curvecp link");
      }
    }
    else if(StrEq(section, "services"))
    {
      if(self->LoadHiddenServiceConfig(val))
      {
        llarp::LogInfo("loaded hidden service config for ", key);
      }
      else
      {
        llarp::LogWarn("failed to load hidden service config for ", key);
      }
    }
    else if(StrEq(section, "connect"))
    {
      self->connect[key] = val;
    }
    else if(StrEq(section, "network"))
    {
    }
    else if(StrEq(section, "router"))
    {
      if(StrEq(key, "nickname"))
      {
        self->rc.SetNick(val);
        // set logger name here
        _glog.nodeName = self->rc.Nick();
      }
      if(StrEq(key, "encryption-privkey"))
      {
        self->encryption_keyfile = val;
      }
      if(StrEq(key, "contact-file"))
      {
        self->our_rc_file = val;
      }
      if(StrEq(key, "transport-privkey"))
      {
        self->transport_keyfile = val;
      }
      if(StrEq(key, "ident-privkey"))
      {
        self->ident_keyfile = val;
      }
      if(StrEq(key, "public-address"))
      {
        llarp::LogInfo("public ip ", val, " size ", strlen(val));
        if(strlen(val) < 17)
        {
          // assume IPv4
          inet_pton(AF_INET, val, &self->ip4addr.sin_addr);
          // struct sockaddr dest;
          sockaddr *dest = (sockaddr *)&self->ip4addr;
          llarp::Addr a(*dest);
          llarp::LogInfo("setting public ipv4 ", a);
          self->addrInfo.ip    = *a.addr6();
          self->publicOverride = true;
        }
        // llarp::Addr a(val);
      }
      if(StrEq(key, "public-port"))
      {
        llarp::LogInfo("Setting public port ", val);
        self->ip4addr.sin_port = htons(atoi(val));
        self->addrInfo.port    = htons(atoi(val));
        self->publicOverride   = true;
      }
    }
  }  // namespace llarp
}  // namespace llarp
