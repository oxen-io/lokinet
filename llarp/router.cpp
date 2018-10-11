#include "router.hpp"
#include <llarp/proto.h>
#include <llarp/iwp.hpp>
#include <llarp/link_message.hpp>
#include <llarp/link/utp.hpp>
#include <llarp/arpc.hpp>

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
    TryConnectJob *establish_job;
  };

}  // namespace llarp

struct TryConnectJob
{
  llarp::RouterContact rc;
  llarp::ILinkLayer *link;
  llarp_router *router;
  uint16_t triesLeft;
  TryConnectJob(const llarp::RouterContact &remote, llarp::ILinkLayer *l,
                uint16_t tries, llarp_router *r)
      : rc(remote), link(l), router(r), triesLeft(tries)
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
  }

  void
  AttemptTimedout()
  {
    router->routerProfiling.MarkTimeout(rc.pubkey);
    if(ShouldRetry())
    {
      Attempt();
      return;
    }
    if(router->routerProfiling.IsBad(rc.pubkey))
      llarp_nodedb_del_rc(router->nodedb, rc.pubkey);
    // delete this
    router->pendingEstablishJobs.erase(rc.pubkey);
  }

  void
  Attempt()
  {
    --triesLeft;
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
  TryConnectJob *j = static_cast< TryConnectJob * >(u);
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

  auto link          = router->outboundLink.get();
  auto itr           = router->pendingEstablishJobs.insert(std::make_pair(
      remote.pubkey,
      std::make_unique< TryConnectJob >(remote, link, numretries, router)));
  TryConnectJob *job = itr.first->second.get();
  // try establishing async
  llarp_logic_queue_job(router->logic, {job, &on_try_connecting});
  return true;
}

void
llarp_router::HandleLinkSessionEstablished(llarp::RouterContact rc)
{
  async_verify_RC(rc);
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
  if(!session)
  {
    llarp::LogWarn("no link session");
    return false;
  }
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

constexpr size_t MaxPendingSendQueueSize = 8;

bool
llarp_router::SendToOrQueue(const llarp::RouterID &remote,
                            const llarp::ILinkMessage *msg)
{
  if(inboundLinks.size() == 0)
  {
    if(outboundLink->HasSessionTo(remote))
    {
      SendTo(remote, msg, outboundLink.get());
      return true;
    }
  }
  else
  {
    for(const auto &link : inboundLinks)
    {
      if(link->HasSessionTo(remote))
      {
        SendTo(remote, msg, link.get());
        return true;
      }
    }
  }
  // no link available

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

  if(q.size() < MaxPendingSendQueueSize)
  {
    buf.sz = buf.cur - buf.base;
    q.emplace(buf.sz);
    memcpy(q.back().data(), buf.base, buf.sz);
  }
  else
  {
    llarp::LogWarn("tried to queue a message to ", remote,
                   " but the queue is full so we drop it like it's hawt");
  }
  llarp::RouterContact remoteRC;
  // we don't have an open session to that router right now
  if(llarp_nodedb_get_rc(nodedb, remote, remoteRC))
  {
    // try connecting directly as the rc is loaded from disk
    llarp_router_try_connect(this, remoteRC, 10);
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
    async_verify_RC(results[0]);
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
    llarp::LogError("failed to verify signature of RC ", rcfile);
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
  if(!rc().VerifySignature(&crypto))
  {
    rc().Dump< MAX_RC_SIZE >();
    llarp::LogError("RC has bad signature not saving");
    return false;
  }
  return rc().Write(our_rc_file.string().c_str());
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
  auto router = ctx->router;
  llarp::PubKey pk(job->rc.pubkey);
  router->FlushOutboundFor(pk, router->GetLinkWithSessionByPubkey(pk));
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

  if(router->validRouters.count(pk))
  {
    router->validRouters.erase(pk);
  }

  llarp::RouterContact rc = job->rc;

  router->validRouters.insert(std::make_pair(pk, rc));

  // track valid router in dht
  router->dht->impl.nodes->PutNode(rc);

  // mark success in profile
  router->routerProfiling.MarkSuccess(pk);

  // this was an outbound establish job
  if(ctx->establish_job)
  {
    ctx->establish_job->Success();
  }
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
  else if(!routerProfiling.IsBad(remote))
  {
    if(dht->impl.HasRouterLookup(remote))
      return;
    llarp::LogInfo("looking up router ", remote);
    // dht lookup as we don't know it
    dht->impl.LookupRouter(
        remote,
        std::bind(&llarp_router::HandleDHTLookupForTryEstablishTo, this, remote,
                  std::placeholders::_1));
  }
}

void
llarp_router::OnConnectTimeout(const llarp::RouterID &remote)
{
  auto itr = pendingEstablishJobs.find(remote);
  if(itr != pendingEstablishJobs.end())
  {
    itr->second->AttemptTimedout();
  }
}

void
llarp_router::HandleDHTLookupForTryEstablishTo(
    llarp::RouterID remote, const std::vector< llarp::RouterContact > &results)
{
  if(results.size() == 0)
  {
    routerProfiling.MarkTimeout(remote);
  }
  for(const auto &result : results)
  {
    llarp_nodedb_put_rc(nodedb, result);
    llarp_router_try_connect(this, result, 10);
    async_verify_RC(result);
  }
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
      if(now < itr->second)
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
      }
      ++itr;
    }
  }

  if(inboundLinks.size() == 0)
  {
    auto N = llarp_nodedb_num_loaded(nodedb);
    if(N < minRequiredRouters)
    {
      llarp::LogInfo("We need at least ", minRequiredRouters,
                     " service nodes to build paths but we have ", N);
      auto explore = std::max(NumberOfConnectedRouters(), size_t(1));
      dht->impl.Explore(explore);
    }
    paths.BuildPaths();
    hiddenServiceContext.Tick();
  }
  if(NumberOfConnectedRouters() < minConnectedRouters)
  {
    ConnectToRandomRouters(minConnectedRouters);
  }
  paths.TickPaths();
}

void
llarp_router::SendTo(llarp::RouterID remote, const llarp::ILinkMessage *msg,
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
  llarp::LogDebug("send ", buf.sz, " bytes to ", remote);
  if(selected)
  {
    if(!selected->SendTo(remote, buf))
      llarp::LogWarn("message to ", remote, " was dropped");
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
  pendingEstablishJobs.erase(remote);
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
llarp_router::async_verify_RC(const llarp::RouterContact &rc)
{
  llarp_async_verify_rc *job       = new llarp_async_verify_rc();
  llarp::async_verify_context *ctx = new llarp::async_verify_context();
  ctx->router                      = this;
  ctx->establish_job               = nullptr;

  auto itr = pendingEstablishJobs.find(rc.pubkey);
  if(itr != pendingEstablishJobs.end())
    ctx->establish_job = itr->second.get();

  job->user  = ctx;
  job->rc    = rc;
  job->valid = false;
  job->hook  = nullptr;

  job->nodedb = nodedb;
  job->logic  = logic;
  // job->crypto = &crypto; // we already have this
  job->cryptoworker = tp;
  job->diskworker   = disk;

  if(rc.IsPublicRouter())
    job->hook = &llarp_router::on_verify_server_rc;
  else
    job->hook = &llarp_router::on_verify_client_rc;
  llarp_nodedb_async_verify(job);
}

void
llarp_router::Run()
{
  if(enableRPCServer)
  {
    if(rpcBindAddr.empty())
    {
      rpcBindAddr = DefaultRPCBindAddr;
    }
    rpcServer = std::make_unique< llarp::arpc::Server >(this);
    if(!rpcServer->Start(rpcBindAddr))
    {
      llarp::LogError("Binding rpc server to ", rpcBindAddr, " failed");
      rpcServer.reset();
    }
    else
      llarp::LogInfo("Bound RPC server to ", rpcBindAddr);
  }

  routerProfiling.Load(routerProfilesFile.c_str());
  // zero out router contact
  const sockaddr *dest = (sockaddr *)&this->ip4addr;
  llarp::Addr publicAddr(*dest);
  if(this->publicOverride)
  {
    llarp::LogDebug("public address:port ", publicAddr);
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
      _rc.addrs.push_back(addr);
    }
  };
  if(this->publicOverride)
  {
    llarp::ILinkLayer *link = nullptr;
    // llarp::LogWarn("Need to load our public IP into RC!");
    if(inboundLinks.size() == 1)
    {
      link = inboundLinks[0].get();
    }
    else
    {
      if(inboundLinks.size())
      {
        link = inboundLinks[0].get();
      }
      else
      {
        llarp::LogWarn(
            "No need to set public ipv4 and port if no external interface "
            "binds, turning off public override");
        this->publicOverride = false;
        link                 = nullptr;
      }
    }
    if(link && link->GetOurAddressInfo(this->addrInfo))
    {
      // override ip and port
      this->addrInfo.ip   = *publicAddr.addr6();
      this->addrInfo.port = publicAddr.port();
      llarp::LogInfo("Loaded our public ", publicAddr, " override into RC!");
      _rc.addrs.push_back(this->addrInfo);
    }
  }

  // set public encryption key
  _rc.enckey = llarp::seckey_topublic(encryption);
  llarp::LogInfo("Your Encryption pubkey ", rc().enckey);
  // set public signing key
  _rc.pubkey = llarp::seckey_topublic(identity);
  llarp::LogInfo("Your Identity pubkey ", rc().pubkey);

  llarp::LogInfo("Signing rc...");
  if(!_rc.Sign(&crypto, identity))
  {
    llarp::LogError("failed to sign rc");
    return;
  }

  if(!SaveRC())
  {
    return;
  }

  llarp::LogInfo("have ", llarp_nodedb_num_loaded(nodedb), " routers");

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
    // we are a client
    // regenerate keys and resign rc before everything else
    crypto.identity_keygen(identity);
    crypto.encryption_keygen(encryption);
    _rc.pubkey = llarp::seckey_topublic(identity);
    _rc.enckey = llarp::seckey_topublic(encryption);
    if(!_rc.Sign(&crypto, identity))
    {
      llarp::LogError("failed to regenerate keys and sign RC");
      return;
    }

    // don't create default if we already have some defined
    if(this->ShouldCreateDefaultHiddenService())
    {
      // generate default hidden service
      if(!CreateDefaultHiddenService())
        return;
    }
    // delayed connect all for clients
    uint64_t delay = ((llarp_randint() % 10) * 500) + 500;
    llarp_logic_call_later(logic, {delay, this, &ConnectAll});
  }

  llarp::PubKey ourPubkey = pubkey();
  llarp::LogInfo("starting dht context as ", ourPubkey);
  llarp_dht_context_start(dht, ourPubkey);

  ScheduleTicker(1000);
}

bool
llarp_router::ShouldCreateDefaultHiddenService()
{
  // llarp::LogInfo("IfName: ", this->defaultIfName, " defaultIfName: ",
  // this->defaultIfName);
  if(this->defaultIfName == "auto" || this->defaultIfName == "auto")
  {
    // auto detect if we have any pre-defined endpoints
    // no if we have a endpoints
    llarp::LogInfo("Auto mode detected, hasEndpoints: ",
                   std::to_string(this->hiddenServiceContext.hasEndpoints()));
    if(this->hiddenServiceContext.hasEndpoints())
      return false;
    // we don't have any endpoints, auto configure settings

    // set a default IP range
    this->defaultIfAddr = llarp::findFreePrivateRange();
    if(this->defaultIfAddr == "")
    {
      llarp::LogError(
          "Could not find any free lokitun interface names, can't auto set up "
          "default HS context for client");
      this->defaultIfAddr = "no";
      return false;
    }

    // pick an ifName
    this->defaultIfName = llarp::findFreeLokiTunIfName();
    if(this->defaultIfName == "")
    {
      llarp::LogError(
          "Could not find any free private ip ranges, can't auto set up "
          "default HS context for client");
      this->defaultIfName = "no";
      return false;
    }
    // auto config'd, go ahead and create it
    return true;
  }
  // not auto mode then just check to make sure it's explicitly disabled
  if(this->defaultIfAddr != "" && this->defaultIfAddr != "no"
     && this->defaultIfName != "" && this->defaultIfName != "no")
  {
    return true;
  }
  return false;
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
  // connect to all explicit connections in connect block
  for(const auto &itr : self->connect)
  {
    llarp::LogInfo("connecting to node ", itr.first);
    self->try_connect(itr.second);
  }
}

bool
llarp_router::HasSessionTo(const llarp::RouterID &remote) const
{
  return validRouters.find(remote) != validRouters.end();
}

void
llarp_router::ConnectToRandomRouters(int want)
{
  int wanted         = want;
  llarp_router *self = this;
  llarp_nodedb_visit_loaded(
      self->nodedb, [self, &want](const llarp::RouterContact &other) -> bool {
        if(llarp_randint() % 2 == 0
           && !(self->HasSessionTo(other.pubkey)
                || self->HasPendingConnectJob(other.pubkey)))
        {
          llarp_router_try_connect(self, other, 5);
          --want;
        }
        return want > 0;
      });
  if(wanted != want)
    llarp::LogInfo("connecting to ", abs(want - wanted), " out of ", wanted,
                   " random routers");
}

bool
llarp_router::ReloadConfig(const llarp_config *conf)
{
  return true;
}

bool
llarp_router::InitOutboundLink()
{
  if(outboundLink)
    return true;

  auto link = llarp::utp::NewServer(this);

  if(!link->EnsureKeys(transport_keyfile.string().c_str()))
  {
    llarp::LogError("failed to load ", transport_keyfile);
    return false;
  }

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
llarp_router::CreateDefaultHiddenService()
{
  return hiddenServiceContext.AddDefaultEndpoint(defaultIfAddr, defaultIfName);
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
  {
    router->Close();
    router->routerProfiling.Save(router->routerProfilesFile.c_str());
  }
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
        auto server = llarp::utp::NewServer(self);
        if(!server->EnsureKeys(self->transport_keyfile.string().c_str()))
        {
          llarp::LogError("failed to ensure keyfile ", self->transport_keyfile);
          return;
        }
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
    else if(StrEq(section, "network"))
    {
      if(StrEq(key, "ifaddr"))
      {
        self->defaultIfAddr = val;
      }
      if(StrEq(key, "ifname"))
      {
        self->defaultIfName = val;
      }
    }
    else if(StrEq(section, "api"))
    {
      if(StrEq(key, "enabled"))
      {
        self->enableRPCServer = IsTrueValue(val);
      }
      if(StrEq(key, "bind"))
      {
        self->rpcBindAddr = val;
      }
      if(StrEq(key, "authkey"))
      {
        // TODO: add pubkey to whitelist
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
      if(StrEq(key, "profiles"))
      {
        self->routerProfilesFile = val;
        self->routerProfiling.Load(val);
        llarp::LogInfo("setting profiles to ", self->routerProfilesFile);
      }
      if(StrEq(key, "min-connected"))
      {
        self->minConnectedRouters = std::max(atoi(val), 0);
      }
      if(StrEq(key, "max-connected"))
      {
        self->maxConnectedRouters = std::max(atoi(val), 1);
      }
    }
    else if(StrEq(section, "router"))
    {
      if(StrEq(key, "nickname"))
      {
        self->_rc.SetNick(val);
        // set logger name here
        _glog.nodeName = self->rc().Nick();
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
          // inet_pton(AF_INET, val, &self->ip4addr.sin_addr);
          // struct sockaddr dest;
          // sockaddr *dest = (sockaddr *)&self->ip4addr;
          llarp::Addr a(val);
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
  }

  namespace arpc
  {
    const byte_t *
    Server::SigningPrivateKey() const
    {
      return router->identity;
    }

    const llarp_crypto *
    Server::Crypto() const
    {
      return &router->crypto;
    }
  }  // namespace arpc
}  // namespace llarp
