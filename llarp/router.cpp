#include "router.hpp"
#include <llarp/iwp.h>
#include <llarp/proto.h>
#include <llarp/link_message.hpp>
#include <llarp/messages/discard.hpp>
#include "llarp/iwp/establish_job.hpp"
#include "llarp/iwp/server.hpp"
#include "llarp/iwp/session.hpp"

#include "buffer.hpp"
#include "encode.hpp"
#include "logger.hpp"
#include "net.hpp"
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
    llarp_link_establish_job *establish_job;
  };

}  // namespace llarp

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
  llarp_rc_clear(&rc);
}

llarp_router::~llarp_router()
{
  llarp_dht_context_free(dht);
  llarp_rc_free(&rc);
}

bool
llarp_router::HandleRecvLinkMessage(llarp_link_session *session,
                                    llarp_buffer_t buf)
{
  return inbound_link_msg_parser.ProcessFrom(session, buf);
}

bool
llarp_router::SendToOrQueue(const llarp::RouterID &remote,
                            const llarp::ILinkMessage *msg)
{
  llarp_link *chosen = nullptr;
  if(!outboundLink->has_session_to(outboundLink, remote))
  {
    for(auto link : inboundLinks)
    {
      if(link->has_session_to(link, remote))
      {
        chosen = link;
        break;
      }
    }
  }
  else
    chosen = outboundLink;

  if(chosen)
  {
    SendTo(remote, msg, chosen);
    delete msg;
    return true;
  }
  // this will create an entry in the obmq if it's not already there
  auto itr = outboundMesssageQueue.find(remote);
  if(itr == outboundMesssageQueue.end())
  {
    outboundMesssageQueue.emplace(std::make_pair(remote, MessageQueue()));
  }
  outboundMesssageQueue[remote].push(msg);

  // we don't have an open session to that router right now
  auto rc = llarp_nodedb_get_rc(nodedb, remote);
  if(rc)
  {
    // try connecting directly as the rc is loaded from disk
    llarp_router_try_connect(this, rc, 10);
    return true;
  }

  // this would never be true, as everything is in memory
  // but we'll keep around if we ever need to swap them out of memory
  // but it's best to keep the paradigm that everythign is in memory at this
  // point in development as it will reduce complexity
  /*
  // try requesting the rc from the disk
  llarp_async_load_rc *job = new llarp_async_load_rc;
  job->diskworker          = disk;
  job->nodedb              = nodedb;
  job->logic               = logic;
  job->user                = this;
  job->hook                = &HandleAsyncLoadRCForSendTo;
  memcpy(job->pubkey, remote, PUBKEYSIZE);
  llarp_nodedb_async_load_rc(job);
  */

  // we don't have the RC locally so do a dht lookup
  llarp_router_lookup_job *lookup = new llarp_router_lookup_job;
  lookup->user                    = this;
  llarp_rc_clear(&lookup->result);
  memcpy(lookup->target, remote, PUBKEYSIZE);
  lookup->hook = &HandleDHTLookupForSendTo;
  llarp_dht_lookup_router(this->dht, lookup);

  return true;
}

/*
void
llarp_router::HandleAsyncLoadRCForSendTo(llarp_async_load_rc *job)
{
  llarp_router *router = static_cast< llarp_router * >(job->user);
  if(job->loaded)
  {
    llarp_router_try_connect(router, &job->rc, 10);
  }
  else
  {
    // we don't have the RC locally so do a dht lookup
    llarp_router_lookup_job *lookup = new llarp_router_lookup_job;
    lookup->user                    = router;
    memcpy(lookup->target, job->pubkey, PUBKEYSIZE);
    lookup->hook = &HandleDHTLookupForSendTo;
    llarp_dht_lookup_router(router->dht, lookup);
  }
  delete job;
}
*/

void
llarp_router::HandleDHTLookupForSendTo(llarp_router_lookup_job *job)
{
  llarp_router *self = static_cast< llarp_router * >(job->user);
  if(job->found)
  {
    llarp_router_try_connect(self, &job->result, 10);
  }
  else
  {
    self->DiscardOutboundFor(job->target);
  }
  delete job;
}

void
llarp_router::try_connect(fs::path rcfile)
{
  llarp_rc *remote = new llarp_rc;
  llarp_rc_new(remote);
  remote = llarp_rc_read(rcfile.c_str());
  if(!remote)
  {
    llarp::LogError("failure to decode or verify of remote RC");
    return;
  }
  if(llarp_rc_verify_sig(&crypto, remote))
  {
    llarp::LogDebug("verified signature");
    if(!llarp_router_try_connect(this, remote, 10))
    {
      // or error?
      llarp::LogWarn("session already made");
    }
  }
  else
    llarp::LogError("failed to verify signature of RC", rcfile);
  llarp_rc_free(remote);
}

bool
llarp_router::EnsureIdentity()
{
  if(!EnsureEncryptionKey())
    return false;
  return llarp_findOrCreateIdentity(&crypto, ident_keyfile.c_str(), identity);
}

bool
llarp_router::EnsureEncryptionKey()
{
  return llarp_findOrCreateEncryption(&crypto, encryption_keyfile.c_str(),
                                      &this->encryption);
}

void
llarp_router::AddInboundLink(struct llarp_link *link)
{
  inboundLinks.push_back(link);
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
  if(!llarp_rc_verify_sig(&crypto, &rc))
  {
    llarp::LogError("RC has bad signature not saving");
    return false;
  }

  byte_t tmp[MAX_RC_SIZE];
  auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);

  if(llarp_rc_bencode(&rc, &buf))
  {
    std::ofstream f(our_rc_file);
    if(f.is_open())
    {
      f.write((char *)buf.base, buf.cur - buf.base);
      llarp::LogInfo("our RC saved to ", our_rc_file.c_str());
      return true;
    }
  }
  llarp::LogError("did not save RC to ", our_rc_file.c_str());
  return false;
}

void
llarp_router::Close()
{
  llarp::LogInfo("Closing ", inboundLinks.size(), " server bindings");
  for(auto link : inboundLinks)
  {
    link->stop_link();
    delete link;
  }
  inboundLinks.clear();

  llarp::LogInfo("Closing LokiNetwork client");
  outboundLink->stop_link();
  delete outboundLink;
  outboundLink = nullptr;
}

void
llarp_router::connect_job_retry(void *user, uint64_t orig, uint64_t left)
{
  if(left)
    return;
  llarp_link_establish_job *job =
      static_cast< llarp_link_establish_job * >(user);
  llarp::Addr remote = job->ai;
  if(job->link)
  {
    llarp::LogInfo("trying to establish session again with ", remote);
    job->link->try_establish(job);
  }
  else
  {
    llarp::LogError("establish session retry failed, no link for ", remote);
  }
}

void
llarp_router::on_verify_client_rc(llarp_async_verify_rc *job)
{
  llarp::async_verify_context *ctx =
      static_cast< llarp::async_verify_context * >(job->user);
  llarp::PubKey pk = job->rc.pubkey;
  llarp_rc_free(&job->rc);
  ctx->router->pendingEstablishJobs.erase(pk);
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
      auto session = ctx->establish_job->session;
      if(session)
        session->close();
    }
    llarp_rc_free(&job->rc);
    router->pendingEstablishJobs.erase(pk);
    router->DiscardOutboundFor(pk);
    return;
  }
  // we're valid, which means it's already been committed to the nodedb

  llarp::LogDebug("rc verified and saved to nodedb");

  // refresh valid routers RC value if it's there
  auto v = router->validRouters.find(pk);
  if(v != router->validRouters.end())
  {
    // free previous RC members
    llarp_rc_free(&v->second);
  }
  router->validRouters[pk] = job->rc;

  // track valid router in dht
  llarp_dht_put_peer(router->dht, &router->validRouters[pk]);

  // this was an outbound establish job
  if(ctx->establish_job)
  {
    auto session = ctx->establish_job->session;
    router->FlushOutboundFor(pk, session->get_parent());
    // this frees the job
    router->pendingEstablishJobs.erase(pk);
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
llarp_router::HandleExploritoryPathBuildStarted(llarp_pathbuild_job *job)
{
  delete job;
}

void
llarp_router::Tick()
{
  // llarp::LogDebug("tick router");

  paths.ExpirePaths();
  // TODO: don't do this if we have enough paths already
  if(inboundLinks.size() == 0)
  {
    auto N = llarp_nodedb_num_loaded(nodedb);
    if(N > 2)
    {
      paths.BuildPaths();
    }
    else
    {
      llarp::LogWarn("not enough nodes known to build exploritory paths, have ",
                     N, " nodes, need 3 now (will be 5 later)");
    }
    hiddenServiceContext.Tick();
  }
  paths.TickPaths();
}

bool
llarp_router::send_padded_message(llarp_link_session_iter *itr,
                                  llarp_link_session *peer)
{
  llarp_router *self = static_cast< llarp_router * >(itr->user);
  llarp::RouterID remote;
  remote = &peer->get_remote_router()->pubkey[0];
  llarp::DiscardMessage msg(2000);

  llarp_buffer_t buf =
      llarp::StackBuffer< decltype(linkmsg_buffer) >(self->linkmsg_buffer);

  if(!msg.BEncode(&buf))
    return false;

  buf.sz  = buf.cur - buf.base;
  buf.cur = buf.base;

  for(size_t idx = 0; idx < 5; ++idx)
  {
    peer->sendto(buf);
  }
  return true;
}

void
llarp_router::SendTo(llarp::RouterID remote, const llarp::ILinkMessage *msg,
                     llarp_link *link)
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
  if(link)
  {
    link->sendto(link, remote, buf);
    return;
  }
  bool sent = outboundLink->sendto(outboundLink, remote, buf);
  if(!sent)
  {
    for(auto link : inboundLinks)
    {
      if(!sent)
      {
        sent = link->sendto(link, remote, buf);
      }
    }
  }
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

  llarp_dht_remove_peer(dht, remote);
  llarp_rc_free(&itr->second);
  validRouters.erase(itr);
}

llarp_link *
llarp_router::GetLinkWithSessionByPubkey(const llarp::RouterID &pubkey)
{
  for(auto &link : inboundLinks)
  {
    if(link->has_session_to(link, pubkey))
      return link;
  }
  if(outboundLink->has_session_to(outboundLink, pubkey))
    return outboundLink;
  return nullptr;
}

void
llarp_router::FlushOutboundFor(const llarp::RouterID &remote,
                               llarp_link *chosen)
{
  llarp::LogDebug("Flush outbound for ", remote);
  auto itr = outboundMesssageQueue.find(remote);
  if(itr == outboundMesssageQueue.end())
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
    auto buf = llarp::StackBuffer< decltype(linkmsg_buffer) >(linkmsg_buffer);

    auto &msg = itr->second.front();

    if(!msg->BEncode(&buf))
    {
      llarp::LogWarn("failed to encode outbound message, buffer size left: ",
                     llarp_buffer_size_left(buf));
      delete msg;
      itr->second.pop();
      continue;
    }
    // set size of message
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    if(!chosen->sendto(chosen, remote, buf))
      llarp::LogWarn("failed to send outboud message to ", remote, " via ",
                     chosen->name());

    delete msg;
    itr->second.pop();
  }
}

void
llarp_router::on_try_connect_result(llarp_link_establish_job *job)
{
  llarp_router *router = static_cast< llarp_router * >(job->user);
  if(job->session)
  {
    // llarp::LogDebug("try_connect got session");
    auto session = job->session;
    router->async_verify_RC(session->get_remote_router(), false, job);
    return;
  }
  // llarp::LogDebug("try_connect no session");
  llarp::PubKey pk = job->pubkey;
  if(job->retries > 0)
  {
    job->retries--;
    job->timeout *= 3;
    job->timeout /= 2;
    llarp::LogInfo("session not established with ", pk, " relaxing timeout to ",
                   job->timeout);
    // exponential backoff
    llarp_logic_call_later(
        router->logic, {job->timeout, job, &llarp_router::connect_job_retry});
  }
  else
  {
    llarp::LogWarn("failed to connect to ", pk,
                   " dropping all pending messages");
    router->DiscardOutboundFor(pk);
    router->pendingEstablishJobs.erase(pk);
  }
}

void
llarp_router::DiscardOutboundFor(const llarp::RouterID &remote)
{
  auto &queue = outboundMesssageQueue[remote];
  while(queue.size())
  {
    delete queue.front();
    queue.pop();
  }
  outboundMesssageQueue.erase(remote);
}

void
llarp_router::async_verify_RC(llarp_rc *rc, bool isExpectingClient,
                              llarp_link_establish_job *establish_job)
{
  llarp_async_verify_rc *job = new llarp_async_verify_rc;
  job->user  = new llarp::async_verify_context{this, establish_job};
  job->rc    = {};
  job->valid = false;
  job->hook  = nullptr;

  job->nodedb = nodedb;
  job->logic  = logic;
  // job->crypto = &crypto; // we already have this
  job->cryptoworker = tp;
  job->diskworker   = disk;

  llarp_rc_copy(&job->rc, rc);
  if(isExpectingClient)
    job->hook = &llarp_router::on_verify_client_rc;
  else
    job->hook = &llarp_router::on_verify_server_rc;

  llarp_nodedb_async_verify(job);
}

#include <string.h>

void
llarp_router::Run()
{
  // zero out router contact
  llarp::Zero(&rc, sizeof(llarp_rc));
  // fill our address list
  rc.addrs         = llarp_ai_list_new();
  bool publicFound = false;

  sockaddr *dest = (sockaddr *)&this->ip4addr;
  llarp::Addr publicAddr(*dest);
  if(this->publicOverride)
  {
    if(publicAddr)
    {
      llarp::LogInfo("public address:port ", publicAddr);
      ;
    }
  }

  llarp::LogInfo("You have ", inboundLinks.size(), " inbound links");
  for(auto link : inboundLinks)
  {
    llarp_ai addr;
    link->get_our_address(&addr);
    llarp::Addr a(addr);
    if(this->publicOverride && a.sameAddr(publicAddr))
    {
      llarp::LogInfo("Found adapter for public address");
      publicFound = true;
    }
    if(a.isPrivate())
    {
      if(!this->publicOverride)
      {
        llarp::LogWarn("Skipping private network link: ", a);
        continue;
      }
    }
    llarp::LogInfo("Loading Addr: ", a, " into our RC");

    llarp_ai_list_pushback(rc.addrs, &addr);
  };
  if(this->publicOverride && !publicFound)
  {
    // llarp::LogWarn("Need to load our public IP into RC!");

    llarp_link *link = nullptr;
    if(inboundLinks.size() == 1)
    {
      link = inboundLinks.front();
    }
    else
    {
      if(!inboundLinks.size())
      {
        llarp::LogError("No inbound links found, aborting");
        return;
      }
      link = inboundLinks.front();
      /*
      // create a new link
      link = new llarp_link;
      llarp::Zero(link, sizeof(llarp_link));

      llarp_iwp_args args = {
        .crypto       = &this->crypto,
        .logic        = this->logic,
        .cryptoworker = this->tp,
        .router       = this,
        .keyfile      = this->transport_keyfile.c_str(),
      };
      iwp_link_init(link, args);
      if(llarp_link_initialized(link))
      {

      }
      */
    }
    link->get_our_address(&this->addrInfo);
    // override ip and port
    this->addrInfo.ip   = *publicAddr.addr6();
    this->addrInfo.port = publicAddr.port();
    llarp::LogInfo("Loaded our public ", publicAddr, " override into RC!");
    // we need the link to set the pubkey
    llarp_ai_list_pushback(rc.addrs, &this->addrInfo);
  }
  // set public encryption key
  llarp_rc_set_pubenckey(&rc, llarp::seckey_topublic(encryption));

  char ftmp[68]      = {0};
  const char *hexKey = llarp::HexEncode< llarp::PubKey, decltype(ftmp) >(
      llarp::seckey_topublic(encryption), ftmp);
  llarp::LogInfo("Your Encryption pubkey ", hexKey);
  // set public signing key
  llarp_rc_set_pubsigkey(&rc, llarp::seckey_topublic(identity));
  hexKey = llarp::HexEncode< llarp::PubKey, decltype(ftmp) >(
      llarp::seckey_topublic(identity), ftmp);
  llarp::LogInfo("Your Identity pubkey ", hexKey);

  llarp_rc_sign(&crypto, identity, &rc);

  if(!SaveRC())
  {
    return;
  }

  llarp::LogDebug("starting outbound link");
  if(!outboundLink->start_link(logic))
  {
    llarp::LogWarn("outbound link failed to start");
  }

  int IBLinksStarted = 0;

  // start links
  for(auto link : inboundLinks)
  {
    if(link->start_link(logic))
    {
      llarp::LogDebug("Link ", link->name(), " started");
      IBLinksStarted++;
    }
    else
      llarp::LogWarn("Link ", link->name(), " failed to start");
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

  llarp_iwp_args args = {
      .crypto       = &crypto,
      .logic        = logic,
      .cryptoworker = tp,
      .router       = this,
      .keyfile      = transport_keyfile.c_str(),
  };

  auto link = new(std::nothrow) llarp_link(args);

  auto afs = {AF_INET, AF_INET6};

  if(link)
  {
    llarp::LogInfo("outbound link initialized");
    for(auto af : afs)
    {
      if(link->configure(netloop, "*", af, 0))
      {
        outboundLink = link;
        llarp::LogInfo("outbound link ready");
        return true;
      }
    }
  }
  delete link;
  llarp::LogError("failed to initialize outbound link");
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
    llarp_crypto_libsodium_init(&router->crypto);
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

bool
llarp_router_try_connect(struct llarp_router *router, struct llarp_rc *remote,
                         uint16_t numretries)
{
  char ftmp[68] = {0};
  const char *hexname =
      llarp::HexEncode< llarp::PubKey, decltype(ftmp) >(remote->pubkey, ftmp);

  // do we already have a pending job for this remote?
  if(router->HasPendingConnectJob(remote->pubkey))
  {
    llarp::LogDebug("We have pending connect jobs to ", hexname);
    return false;
  }
  // try first address only
  llarp_ai addr;
  if(llarp_ai_list_index(remote->addrs, 0, &addr))
  {
    auto link = router->outboundLink;
    auto itr  = router->pendingEstablishJobs.emplace(
        std::make_pair(remote->pubkey, llarp_link_establish_job()));
    auto job = &itr.first->second;
    llarp_ai_copy(&job->ai, &addr);
    memcpy(job->pubkey, remote->pubkey, PUBKEYSIZE);
    job->retries = numretries;
    job->timeout = 10000;
    job->result  = &llarp_router::on_try_connect_result;
    // give router as user pointer
    job->user = router;
    // try establishing
    link->try_establish(job);
    return true;
  }
  llarp::LogWarn("couldn't get first address for ", hexname);
  return false;
}

void
llarp_rc_clear(struct llarp_rc *rc)
{
  // zero out router contact
  llarp::Zero(rc, sizeof(llarp_rc));
}

void
llarp_rc_set_pubenckey(struct llarp_rc *rc, const uint8_t *pubenckey)
{
  // set public encryption key
  memcpy(rc->enckey, pubenckey, PUBKEYSIZE);
}

void
llarp_rc_set_pubsigkey(struct llarp_rc *rc, const uint8_t *pubsigkey)
{
  // set public signing key
  memcpy(rc->pubkey, pubsigkey, PUBKEYSIZE);
}

void
llarp_rc_set_pubkey(struct llarp_rc *rc, const uint8_t *pubenckey,
                    const uint8_t *pubsigkey)
{
  // set public encryption key
  llarp_rc_set_pubenckey(rc, pubenckey);
  // set public signing key
  llarp_rc_set_pubsigkey(rc, pubsigkey);
}

struct llarp_rc *
llarp_rc_read(const char *fpath)
{
  fs::path our_rc_file(fpath);
  std::error_code ec;
  if(!fs::exists(our_rc_file, ec))
  {
    printf("File[%s] not found\n", fpath);
    return 0;
  }
  std::ifstream f(our_rc_file, std::ios::binary);
  if(!f.is_open())
  {
    printf("Can't open file [%s]\n", fpath);
    return 0;
  }
  byte_t tmp[MAX_RC_SIZE];
  llarp_buffer_t buf = llarp::StackBuffer< decltype(tmp) >(tmp);
  f.seekg(0, std::ios::end);
  size_t sz = f.tellg();
  f.seekg(0, std::ios::beg);

  if(sz > buf.sz)
    return 0;

  f.read((char *)buf.base, sz);
  // printf("contents[%s]\n", tmpc);
  llarp_rc *rc = new llarp_rc;
  llarp::Zero(rc, sizeof(llarp_rc));
  if(!llarp_rc_bdecode(rc, &buf))
  {
    printf("Can't decode [%s]\n", fpath);
    return 0;
  }
  return rc;
}

bool
llarp_rc_addr_list_iter(struct llarp_ai_list_iter *iter, struct llarp_ai *ai)
{
  struct llarp_rc *rc = (llarp_rc *)iter->user;
  llarp_ai_list_pushback(rc->addrs, ai);
  return true;
}

void
llarp_rc_set_addrs(struct llarp_rc *rc, struct llarp_alloc *mem,
                   struct llarp_ai_list *addr)
{
  rc->addrs = llarp_ai_list_new();
  struct llarp_ai_list_iter ai_itr;
  ai_itr.user  = rc;
  ai_itr.visit = &llarp_rc_addr_list_iter;
  llarp_ai_list_iterate(addr, &ai_itr);
}

bool
llarp_rc_write(struct llarp_rc *rc, const char *fpath)
{
  fs::path our_rc_file(fpath);
  byte_t tmp[MAX_RC_SIZE];
  auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);

  if(llarp_rc_bencode(rc, &buf))
  {
    std::ofstream f(our_rc_file, std::ios::binary);
    if(f.is_open())
    {
      f.write((char *)buf.base, buf.cur - buf.base);
      return true;
    }
  }
  return false;
}

void
llarp_rc_sign(llarp_crypto *crypto, const byte_t *seckey, struct llarp_rc *rc)
{
  byte_t buf[MAX_RC_SIZE];
  auto signbuf = llarp::StackBuffer< decltype(buf) >(buf);
  // zero out previous signature
  llarp::Zero(rc->signature, sizeof(rc->signature));
  // encode
  if(llarp_rc_bencode(rc, &signbuf))
  {
    // sign
    signbuf.sz = signbuf.cur - signbuf.base;
    crypto->sign(rc->signature, seckey, signbuf);
  }
}

void
llarp_stop_router(struct llarp_router *router)
{
  if(router)
    router->Close();
}

void
llarp_router_iterate_links(struct llarp_router *router,
                           struct llarp_router_link_iter i)
{
  for(auto link : router->inboundLinks)
    if(!i.visit(&i, router, link))
      return;
  i.visit(&i, router, router->outboundLink);
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

void
llarp_router_override_path_selection(struct llarp_router *router,
                                     llarp_pathbuilder_select_hop_func func)
{
  if(func)
    router->selectHopFunc = func;
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
    std::ofstream f(path, std::ios::binary);
    if(f.is_open())
    {
      f.write((char *)secretkey, SECKEYSIZE);
    }
  }
  std::ifstream f(path, std::ios::binary);
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
                             llarp::SecretKey *encryption)
{
  llarp::LogDebug("find or create ", fpath);
  fs::path path(fpath);
  std::error_code ec;
  if(!fs::exists(path, ec))
  {
    llarp::LogInfo("generating new encryption key");
    crypto->encryption_keygen(*encryption);
    std::ofstream f(path, std::ios::binary);
    if(f.is_open())
    {
      f.write((char *)encryption, SECKEYSIZE);
    }
  }
  std::ifstream f(path, std::ios::binary);
  if(f.is_open())
  {
    f.read((char *)encryption, SECKEYSIZE);
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

    struct llarp_link *link = nullptr;
    if(StrEq(section, "bind"))
    {
      if(!StrEq(key, "*"))
      {
        llarp::LogInfo("interface specific binding activated");
        llarp_iwp_args args = {
            .crypto       = &self->crypto,
            .logic        = self->logic,
            .cryptoworker = self->tp,
            .router       = self,
            .keyfile      = self->transport_keyfile.c_str(),
        };

        link = new(std::nothrow) llarp_link(args);

        if(link)
        {
          llarp::LogInfo("link ", key, " initialized");
          if(link->configure(self->netloop, key, af, proto))
          {
            self->AddInboundLink(link);
            return;
          }
          if(af == AF_INET6)
          {
            // we failed to configure IPv6
            // try IPv4
            llarp::LogInfo("link ", key,
                           " failed to configure IPv6, trying IPv4");
            af = AF_INET;
            if(link->configure(self->netloop, key, af, proto))
            {
              self->AddInboundLink(link);
              return;
            }
          }
        }
        else
        {
          llarp::LogError("link ", key, " failed to initialize. Link state",
                          link);
        }
      }
      llarp::LogError("link ", key,
                      " failed to configure. (Note: We don't support * yet)");
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
    else if(StrEq(section, "router"))
    {
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
  }
}  // namespace llarp
