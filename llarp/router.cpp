#include "router.hpp"
#include <llarp/iwp.h>
#include <llarp/link.h>
#include <llarp/proto.h>
#include <llarp/router.h>
#include <llarp/link_message.hpp>
#include <llarp/messages/discard.hpp>

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
    , inbound_msg_parser(this)

{
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
  return inbound_msg_parser.ProcessFrom(session, buf);
}

bool
llarp_router::SendToOrQueue(const llarp::RouterID &remote,
                            std::vector< llarp::ILinkMessage * > msgs)
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

  for(const auto &msg : msgs)
  {
    // this will create an entry in the obmq if it's not already there
    outboundMesssageQueue[remote].push(msg);
  }

  if(!chosen)
  {
    // we don't have an open session to that router right now
    auto rc = llarp_nodedb_get_rc(nodedb, remote);
    if(rc)
    {
      // try connecting directly as the rc is loaded from disk
      llarp_router_try_connect(this, rc, 10);
      return true;
    }
    // try requesting the rc from the disk
    llarp_async_load_rc *job = new llarp_async_load_rc;
    job->diskworker          = disk;
    job->nodedb              = nodedb;
    job->logic               = logic;
    job->user                = this;
    job->hook                = &HandleAsyncLoadRCForSendTo;
    memcpy(job->pubkey, remote, PUBKEYSIZE);
    llarp_nodedb_async_load_rc(job);
  }
  else
    FlushOutboundFor(remote, chosen);
  return true;
}

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
    memcpy(lookup->target, job->rc.pubkey, PUBKEYSIZE);
    lookup->hook = &HandleDHTLookupForSendTo;
    llarp_dht_lookup_router(router->dht, lookup);
  }
  delete job;
}

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
  byte_t tmp[MAX_RC_SIZE];
  llarp_rc remote = {0};
  llarp_buffer_t buf;
  llarp::StackBuffer< decltype(tmp) >(buf, tmp);
  // open file
  {
    std::ifstream f(rcfile, std::ios::binary);
    if(f.is_open())
    {
      f.seekg(0, std::ios::end);
      size_t sz = f.tellg();
      f.seekg(0, std::ios::beg);
      if(sz <= buf.sz)
      {
        f.read((char *)buf.base, sz);
      }
      else
        llarp::Error(rcfile, " too large");
    }
    else
    {
      llarp::Error("failed to open ", rcfile);
      return;
    }
  }
  if(llarp_rc_bdecode(&remote, &buf))
  {
    if(llarp_rc_verify_sig(&crypto, &remote))
    {
      llarp::Debug("verified signature");
      if(!llarp_router_try_connect(this, &remote, 10))
      {
        llarp::Warn("session already made");
      }
    }
    else
      llarp::Error("failed to verify signature of RC");
  }
  else
    llarp::Error("failed to decode RC");

  llarp_rc_free(&remote);
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
  std::error_code ec;
  if(!fs::exists(encryption_keyfile, ec))
  {
    llarp::Info("generating encryption key");
    crypto.encryption_keygen(encryption);
    std::ofstream f(encryption_keyfile, std::ios::binary);
    if(!f.is_open())
    {
      llarp::Error("could not save encryption private key to ",
                   encryption_keyfile, " ", ec);
      return false;
    }
    f.write((char *)encryption.data(), encryption.size());
  }
  std::ifstream f(encryption_keyfile, std::ios::binary);
  if(!f.is_open())
  {
    llarp::Error("could not read ", encryption_keyfile);
    return false;
  }
  f.read((char *)encryption.data(), encryption.size());
  return true;
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
  llarp::Debug("verify RC signature");
  if(!llarp_rc_verify_sig(&crypto, &rc))
  {
    llarp::Error("RC has bad signature not saving");
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
      llarp::Info("our RC saved to ", our_rc_file.c_str());
      return true;
    }
  }
  llarp::Error("did not save RC to ", our_rc_file.c_str());
  return false;
}

void
llarp_router::Close()
{
  for(auto link : inboundLinks)
  {
    link->stop_link(link);
    link->free_impl(link);
    delete link;
  }
  inboundLinks.clear();

  outboundLink->stop_link(outboundLink);
  outboundLink->free_impl(outboundLink);
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
  llarp::Info("trying to establish session again with ", remote);
  job->link->try_establish(job->link, job);
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
  llarp::Debug("rc verified? ", job->valid ? "valid" : "invalid");
  llarp::PubKey pk(job->rc.pubkey);
  if(!job->valid)
  {
    llarp::Warn("invalid server RC");
    if(ctx->establish_job)
    {
      // was an outbound attempt
      auto session = ctx->establish_job->session;
      if(session)
        session->close(session);
    }
    llarp_rc_free(&job->rc);
    router->pendingEstablishJobs.erase(pk);
    router->DiscardOutboundFor(pk);
    return;
  }

  llarp::Debug("rc verified");

  // refresh valid routers RC value if it's there
  auto v = router->validRouters.find(pk);
  if(v != router->validRouters.end())
  {
    // free previous RC members
    llarp_rc_free(&v->second);
  }
  router->validRouters[pk] = job->rc;

  // TODO: update nodedb here (?)

  // track valid router in dht
  llarp_dht_put_peer(router->dht, &router->validRouters[pk]);

  // this was an outbound establish job
  if(ctx->establish_job->session)
  {
    auto session = ctx->establish_job->session;
    router->FlushOutboundFor(pk, session->get_parent(session));
    // this frees the job
    router->pendingEstablishJobs.erase(pk);
  }
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
llarp_router::Tick()
{
  llarp::Debug("tick router");
  paths.ExpirePaths();
  llarp_pathbuild_job job;
  llarp_pathbuilder_build_path(&job);
  llarp_link_session_iter iter;
  iter.user  = this;
  iter.visit = &send_padded_message;
  if(sendPadding)
  {
    outboundLink->iter_sessions(outboundLink, iter);
  }
}

bool
llarp_router::send_padded_message(llarp_link_session_iter *itr,
                                  llarp_link_session *peer)
{
  llarp_router *self = static_cast< llarp_router * >(itr->user);
  llarp::RouterID remote;
  remote = &peer->get_remote_router(peer)->pubkey[0];
  llarp::DiscardMessage msg(2000);

  llarp_buffer_t buf =
      llarp::StackBuffer< decltype(linkmsg_buffer) >(self->linkmsg_buffer);

  if(!msg.BEncode(&buf))
    return false;

  buf.sz  = buf.cur - buf.base;
  buf.cur = buf.base;

  for(size_t idx = 0; idx < 5; ++idx)
  {
    peer->sendto(peer, buf);
  }
  return true;
}

void
llarp_router::SendTo(llarp::RouterID remote, llarp::ILinkMessage *msg)
{
  llarp_buffer_t buf =
      llarp::StackBuffer< decltype(linkmsg_buffer) >(linkmsg_buffer);

  if(!msg->BEncode(&buf))
  {
    llarp::Warn("failed to encode outbound message, buffer size left: ",
                llarp_buffer_size_left(buf));
    return;
  }
  // set size of message
  buf.sz  = buf.cur - buf.base;
  buf.cur = buf.base;

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

void
llarp_router::FlushOutboundFor(const llarp::RouterID &remote,
                               llarp_link *chosen)
{
  llarp::Debug("Flush outbound for ", remote);
  auto itr = outboundMesssageQueue.find(remote);
  if(itr == outboundMesssageQueue.end())
    return;
  while(itr->second.size())
  {
    auto buf = llarp::StackBuffer< decltype(linkmsg_buffer) >(linkmsg_buffer);

    auto &msg = itr->second.front();

    if(!msg->BEncode(&buf))
    {
      llarp::Warn("failed to encode outbound message, buffer size left: ",
                  llarp_buffer_size_left(buf));
      delete msg;
      itr->second.pop();
      continue;
    }
    // set size of message
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    if(!chosen->sendto(chosen, remote, buf))
      llarp::Warn("failed to send outboud message to ", remote, " via ",
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
    auto session = job->session;
    router->async_verify_RC(session, false, job);
    return;
  }
  llarp::PubKey pk = job->pubkey;
  if(job->retries > 0)
  {
    job->retries--;
    job->timeout *= 3;
    job->timeout /= 2;
    llarp::Info("session not established with ", pk, " relaxing timeout to ",
                job->timeout);
    // exponential backoff
    llarp_logic_call_later(
        router->logic, {job->timeout, job, &llarp_router::connect_job_retry});
  }
  else
  {
    llarp::Warn("failed to connect to ", pk, " dropping all pending messages");
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
llarp_router::async_verify_RC(llarp_link_session *session,
                              bool isExpectingClient,
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

  llarp_rc_copy(&job->rc, session->get_remote_router(session));
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
  llarp::Zero(&rc, sizeof(llarp_rc));
  // fill our address list
  rc.addrs = llarp_ai_list_new();
  for(auto link : inboundLinks)
  {
    llarp_ai addr;
    link->get_our_address(link, &addr);
    llarp_ai_list_pushback(rc.addrs, &addr);
  };
  // set public keys
  memcpy(rc.enckey, llarp::seckey_topublic(encryption), PUBKEYSIZE);
  llarp_rc_set_pubkey(&rc, pubkey());

  llarp_rc_sign(&crypto, identity, &rc);

  if(!SaveRC())
  {
    return;
  }

  llarp::Debug("starting outbound link");
  if(!outboundLink->start_link(outboundLink, logic))
  {
    llarp::Warn("outbound link failed to start");
  }

  int IBLinksStarted = 0;

  // start links
  for(auto link : inboundLinks)
  {
    if(link->start_link(link, logic))
    {
      llarp::Debug("Link ", link->name(), " started");
      IBLinksStarted++;
    }
    else
      llarp::Warn("Link ", link->name(), " failed to start");
  }

  if(IBLinksStarted > 0)
  {
    // initialize as service node
    InitServiceNode();
  }

  llarp::PubKey ourPubkey = pubkey();
  llarp::Info("starting dht context as ", ourPubkey);
  llarp_dht_context_start(dht, ourPubkey);

  llarp_logic_call_later(logic, {1000, this, &ConnectAll});

  ScheduleTicker(500);
}

void
llarp_router::InitServiceNode()
{
  llarp::Info("accepting transit traffic");
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
    llarp::Info("connecting to node ", itr.first);
    self->try_connect(itr.second);
  }
}
bool
llarp_router::InitOutboundLink()
{
  if(outboundLink)
    return true;
  auto link = new llarp_link;
  llarp::Zero(link, sizeof(llarp_link));

  llarp_iwp_args args = {
      .crypto       = &crypto,
      .logic        = logic,
      .cryptoworker = tp,
      .router       = this,
      .keyfile      = transport_keyfile.c_str(),
  };
  auto afs = {AF_INET, AF_INET6};
  iwp_link_init(link, args);
  if(llarp_link_initialized(link))
  {
    llarp::Info("outbound link initialized");
    for(auto af : afs)
    {
      if(link->configure(link, netloop, "*", af, 0))
      {
        outboundLink = link;
        llarp::Info("outbound link ready");
        return true;
      }
    }
  }
  delete link;
  llarp::Error("failed to initialize outbound link");
  return false;
}

bool
llarp_router::HasPendingConnectJob(const llarp::RouterID &remote)
{
  return pendingEstablishJobs.find(remote) != pendingEstablishJobs.end();
}

extern "C" {
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
  // do  we already have a pending job for this remote?
  if(router->HasPendingConnectJob(remote->pubkey))
    return false;
  // try first address only
  llarp_ai addr;
  if(llarp_ai_list_index(remote->addrs, 0, &addr))
  {
    auto link = router->outboundLink;
    auto itr  = router->pendingEstablishJobs.emplace(
        std::make_pair(remote->pubkey, llarp_link_establish_job{}));
    auto job = &itr.first->second;
    llarp_ai_copy(&job->ai, &addr);
    memcpy(job->pubkey, remote->pubkey, PUBKEYSIZE);
    job->retries = numretries;
    job->timeout = 10000;
    job->result  = &llarp_router::on_try_connect_result;
    // give router as user pointer
    job->user = router;
    // try establishing
    link->try_establish(link, job);
    return true;
  }
  return false;
}

void
llarp_rc_clear(struct llarp_rc *rc)
{
  // zero out router contact
  llarp::Zero(rc, sizeof(llarp_rc));
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

void
llarp_rc_set_pubkey(struct llarp_rc *rc, const uint8_t *pubkey)
{
  // set public key
  memcpy(rc->pubkey, pubkey, 32);
}

bool
llarp_findOrCreateIdentity(llarp_crypto *crypto, const char *fpath,
                           byte_t *secretkey)
{
  llarp::Debug("find or create ", fpath);
  fs::path path(fpath);
  std::error_code ec;
  if(!fs::exists(path, ec))
  {
    llarp::Info("regenerated identity key");
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
  llarp::Info("failed to get identity key");
  return false;
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
  //printf("contents[%s]\n", tmpc);
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
        link = new llarp_link;
        llarp::Zero(link, sizeof(llarp_link));

        llarp_iwp_args args = {
            .crypto       = &self->crypto,
            .logic        = self->logic,
            .cryptoworker = self->tp,
            .router       = self,
            .keyfile      = self->transport_keyfile.c_str(),
        };
        iwp_link_init(link, args);
        if(llarp_link_initialized(link))
        {
          llarp::Info("link ", key, " initialized");
          if(link->configure(link, self->netloop, key, af, proto))
          {
            self->AddInboundLink(link);
            return;
          }
          if(af == AF_INET6)
          {
            // we failed to configure IPv6
            // try IPv4
            llarp::Info("link ", key, " failed to configure IPv6, trying IPv4");
            af = AF_INET;
            if(link->configure(link, self->netloop, key, af, proto))
            {
              self->AddInboundLink(link);
              return;
            }
          }
        }
      }
      llarp::Error("link ", key, " failed to configure");
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
    }
  }

}  // namespace llarp
