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
    : ready(false), dht(llarp_dht_context_new(this)), inbound_msg_parser(this)
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
  bool has = false;
  for(auto &link : links)
    has |= link->has_session_to(link, remote);

  if(!has)
  {
    llarp_rc rc;
    llarp_rc_clear(&rc);
    llarp_pubkey_t k;
    memcpy(k, remote, sizeof(llarp_pubkey_t));

    if(!llarp_nodedb_find_rc(nodedb, &rc, k))
    {
      llarp::Warn("cannot find router ", remote, " locally so we are dropping ",
                  msgs.size(), " messages to them");

      for(auto &msg : msgs)
        delete msg;

      msgs.clear();
      return false;
    }

    for(const auto &msg : msgs)
    {
      // this will create an entry in the obmq if it's not already there
      outboundMesssageQueue[remote].push(msg);
    }
    // queued
    llarp_router_try_connect(this, &rc);
    llarp_rc_clear(&rc);
    return true;
  }

  for(const auto &msg : msgs)
  {
    outboundMesssageQueue[remote].push(msg);
  }
  FlushOutboundFor(remote);
  return true;
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
      if(!llarp_router_try_connect(this, &remote))
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
  return llarp_findOrCreateIdentity(&crypto, ident_keyfile.c_str(), identity);
}

void
llarp_router::AddLink(struct llarp_link *link)
{
  links.push_back(link);
  ready = true;
}

bool
llarp_router::Ready()
{
  return ready;
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
  for(auto &link : links)
  {
    link->stop_link(link);
    link->free_impl(link);
    delete link;
  }
  links.clear();
}

void
llarp_router::connect_job_retry(void *user)
{
  llarp_link_establish_job *job =
      static_cast< llarp_link_establish_job * >(user);

  llarp::Info("trying to establish session again");
  job->link->try_establish(job->link, job);
}

void
llarp_router::on_verify_client_rc(llarp_async_verify_rc *job)
{
  llarp::async_verify_context *ctx =
      static_cast< llarp::async_verify_context * >(job->user);
  llarp_rc_free(&job->rc);
  delete ctx;
}

void
llarp_router::on_verify_server_rc(llarp_async_verify_rc *job)
{
  llarp::async_verify_context *ctx =
      static_cast< llarp::async_verify_context * >(job->user);
  auto router = ctx->router;
  if(!job->valid)
  {
    llarp::Warn("invalid server RC");
    if(ctx->establish_job)
    {
      // was an outbound attempt
      auto session = ctx->establish_job->session;
      if(session)
        session->close(session);
      delete ctx->establish_job;
    }
    llarp_rc_free(&job->rc);
    delete job;
    return;
  }

  llarp::Debug("rc verified");

  // track valid router in dht
  llarp::pubkey pubkey;
  memcpy(&pubkey[0], job->rc.pubkey, pubkey.size());

  // refresh valid routers RC value if it's there
  auto v = router->validRouters.find(pubkey);
  if(v != router->validRouters.end())
  {
    // free previous RC members
    llarp_rc_free(&v->second);
  }
  router->validRouters[pubkey] = job->rc;

  // track valid router in dht
  llarp_dht_put_local_router(router->dht, &router->validRouters[pubkey]);

  // this was an outbound establish job
  if(ctx->establish_job->session)
  {
    router->FlushOutboundFor(pubkey);
  }
  llarp_rc_free(&job->rc);
  delete job;
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
  if(sendPadding)
  {
    for(auto &link : links)
    {
      link->iter_sessions(link, {this, nullptr, &send_padded_message});
    }
  }
}

bool
llarp_router::send_padded_message(llarp_link_session_iter *itr,
                                  llarp_link_session *peer)
{
  auto msg           = new llarp::DiscardMessage({}, 4096);
  llarp_router *self = static_cast< llarp_router * >(itr->user);
  self->SendToOrQueue(peer->get_remote_router(peer)->pubkey, {msg});
  return true;
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
  llarp_dht_remove_local_router(dht, remote);
  llarp_rc_free(&itr->second);
  validRouters.erase(itr);
}

void
llarp_router::FlushOutboundFor(const llarp::RouterID &remote)
{
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

    bool sent = false;
    for(auto &link : links)
    {
      if(!sent)
      {
        sent = link->sendto(link, remote, buf);
      }
    }
    if(!sent)
    {
      llarp::Warn("failed to flush outboud message queue for ", remote);
    }
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
  llarp::Info("session not established");
  llarp_logic_queue_job(router->logic, {job, &llarp_router::connect_job_retry});
}
void
llarp_router::async_verify_RC(llarp_link_session *session,
                              bool isExpectingClient,
                              llarp_link_establish_job *establish_job)
{
  llarp_async_verify_rc *job = new llarp_async_verify_rc{
      new llarp::async_verify_context{this, establish_job},
      {},
      false,
      nullptr,
  };
  llarp_rc_copy(&job->rc, session->get_remote_router(session));
  if(isExpectingClient)
    job->hook = &llarp_router::on_verify_client_rc;
  else
    job->hook = &llarp_router::on_verify_server_rc;

  llarp_nodedb_async_verify(nodedb, logic, &crypto, tp, disk, job);
}

void
llarp_router::Run()
{
  // zero out router contact
  llarp::Zero(&rc, sizeof(llarp_rc));
  // fill our address list
  rc.addrs = llarp_ai_list_new();
  for(auto link : links)
  {
    llarp_ai addr;
    link->get_our_address(link, &addr);
    llarp_ai_list_pushback(rc.addrs, &addr);
  };
  // set public key
  llarp_rc_set_pubkey(&rc, pubkey());

  llarp_rc_sign(&crypto, identity, &rc);

  if(!SaveRC())
  {
    return;
  }

  llarp::pubkey ourPubkey = pubkey();

  llarp::Info("our router has public key ", ourPubkey);
  llarp_dht_context_start(dht, ourPubkey);

  // start links
  for(auto link : links)
  {
    int result = link->start_link(link, logic);
    if(result == -1)
      llarp::Warn("Link ", link->name(), " failed to start");
    else
      llarp::Debug("Link ", link->name(), " started");
  }

  for(const auto &itr : connect)
  {
    llarp::Info("connecting to node ", itr.first);
    try_connect(itr.second);
  }

  ScheduleTicker(500);
}

bool
llarp_router::iter_try_connect(llarp_router_link_iter *iter,
                               llarp_router *router, llarp_link *link)
{
  if(!link)
    return true;

  llarp_link_establish_job *job = new llarp_link_establish_job;

  if(!job)
    return true;
  llarp_ai *ai = static_cast< llarp_ai * >(iter->user);
  llarp_ai_copy(&job->ai, ai);
  job->timeout = 10000;
  job->result  = &llarp_router::on_try_connect_result;
  // give router as user pointer
  job->user = router;
  link->try_establish(link, job);
  // break iteration
  return false;
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
    router->disk = llarp_init_threadpool(1, "llarp-diskio");
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
llarp_router_try_connect(struct llarp_router *router, struct llarp_rc *remote)
{
  // try first address only
  llarp_ai addr;
  if(llarp_ai_list_index(remote->addrs, 0, &addr))
  {
    llarp_router_iterate_links(router,
                               {&addr, &llarp_router::iter_try_connect});
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
llarp_rc_set_pubkey(struct llarp_rc *rc, uint8_t *pubkey)
{
  // set public key
  memcpy(rc->pubkey, pubkey, 32);
}

bool
llarp_findOrCreateIdentity(llarp_crypto *crypto, const char *fpath,
                           byte_t *secretkey)
{
  fs::path path(fpath);
  std::error_code ec;
  if(!fs::exists(path, ec))
  {
    crypto->identity_keygen(secretkey);
    std::ofstream f(path, std::ios::binary);
    if(f.is_open())
    {
      f.write((char *)secretkey, sizeof(llarp_seckey_t));
    }
  }
  std::ifstream f(path, std::ios::binary);
  if(f.is_open())
  {
    f.read((char *)secretkey, sizeof(llarp_seckey_t));
    return true;
  }
  return false;
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
  for(auto link : router->links)
    if(!i.visit(&i, router, link))
      return;
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
    if(StrEq(section, "iwp-links"))
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
        // printf("router -> link initialized\n");
        if(link->configure(link, self->netloop, key, af, proto))
        {
          llarp_ai ai;
          link->get_our_address(link, &ai);
          llarp::Addr addr = ai;
          self->AddLink(link);
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
            llarp_ai ai;
            link->get_our_address(link, &ai);
            llarp::Addr addr = ai;
            self->AddLink(link);
            return;
          }
        }
      }
      llarp::Error("link ", key, " failed to configure");
    }
    else if(StrEq(section, "iwp-connect"))
    {
      self->connect[key] = val;
    }
    else if(StrEq(section, "router"))
    {
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
