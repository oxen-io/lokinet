#include "router.hpp"
#include <llarp/dtls.h>
#include <llarp/ibfq.h>
#include <llarp/iwp.h>
#include <llarp/link.h>
#include <llarp/proto.h>
#include <llarp/router.h>
#include "buffer.hpp"
#include "net.hpp"
#include "str.hpp"

#include <fstream>

namespace llarp
{
  void
  router_iter_config(llarp_config_iterator *iter, const char *section,
                     const char *key, const char *val);
}  // namespace llarp

llarp_router::llarp_router(struct llarp_alloc *m) : ready(false), mem(m)
{
  llarp_msg_muxer_init(&muxer);
}

llarp_router::~llarp_router()
{
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
        printf("file too large\n");
    }
    else
    {
      return;
    }
  }
  if(llarp_rc_bdecode(mem, &remote, &buf))
  {
    if(llarp_rc_verify_sig(&crypto, &remote))
    {
      printf("signature valided\n");
      if(llarp_router_try_connect(this, &remote))
      {
        printf("session attempt started\n");
      }
      else
      {
        printf("session already pending\n");
      }
    }
    else
      printf("failed to verify signature\n");
  }
  else
    printf("failed to decode buffer, read=%ld\n", buf.cur - buf.base);

  llarp_rc_free(&remote);
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
llarp_router::EnsureIdentity()
{
  std::error_code ec;
  if(!fs::exists(ident_keyfile, ec))
  {
    crypto.keygen(identity);
    std::ofstream f(ident_keyfile, std::ios::binary);
    if(f.is_open())
    {
      f.write((char *)identity, sizeof(identity));
    }
  }
  std::ifstream f(ident_keyfile, std::ios::binary);
  if(f.is_open())
  {
    f.read((char *)identity, sizeof(identity));
    return true;
  }
  return false;
}

bool
llarp_router::SaveRC()
{
  printf("verify rc signature... ");
  if(!llarp_rc_verify_sig(&crypto, &rc))
  {
    printf(" BAD!\n");
    return false;
  }
  printf(" OK.\n");

  byte_t tmp[MAX_RC_SIZE];
  llarp_buffer_t buf;
  llarp::StackBuffer< decltype(tmp) >(buf, tmp);

  if(llarp_rc_bencode(&rc, &buf))
  {
    std::ofstream f(our_rc_file);
    if(f.is_open())
    {
      f.write((char *)buf.base, buf.cur - buf.base);
      return true;
    }
  }
  return false;
}

void
llarp_router::Close()
{
  for(auto link : links)
  {
    link->stop_link(link);
  }
}

void
llarp_router::on_try_connect_result(llarp_link_establish_job *job)
{
  printf("on_try_connect_result\n");
  llarp_router *self = static_cast< llarp_router * >(job->user);
  if(job->session)
    printf("session made\n");
  else
    printf("session not made\n");
  self->mem->free(self->mem, job);
}

void
llarp_router::Run()
{
  // zero out router contact
  llarp::Zero(&rc, sizeof(llarp_rc));
  // fill our address list
  rc.addrs = llarp_ai_list_new(mem);
  llarp_ai addr;
  for(auto link : links)
  {
    link->get_our_address(link, &addr);
    llarp_ai_list_pushback(rc.addrs, &addr);
  };
  // set public key
  memcpy(rc.pubkey, pubkey(), 32);
  {
    // sign router contact
    byte_t rcbuf[MAX_RC_SIZE];
    llarp_buffer_t signbuf;
    llarp::StackBuffer< decltype(rcbuf) >(signbuf, rcbuf);
    // encode
    if(!llarp_rc_bencode(&rc, &signbuf))
      return;

    // sign
    signbuf.sz = signbuf.cur - signbuf.base;
    printf("sign %ld bytes\n", signbuf.sz);
    crypto.sign(rc.signature, identity, signbuf);
  }

  if(!SaveRC())
    return;

  printf("saved router contact\n");
  // start links
  for(auto link : links)
  {
    int result = link->start_link(link, logic);
    if(result == -1)
      printf("link %s failed to start\n", link->name());
    else
      printf("link %s started\n", link->name());
  }

  printf("connecting to routers\n");
  for(const auto &itr : connect)
  {
    printf("try connecting to %s\n", itr.first.c_str());
    try_connect(itr.second);
  }
}

bool
llarp_router::iter_try_connect(llarp_router_link_iter *iter,
                               llarp_router *router, llarp_link *link)
{
  if(!link)
    return false;

  auto mem = router->mem;

  llarp_link_establish_job *job = llarp::Alloc< llarp_link_establish_job >(mem);

  if(!job)
    return false;
  llarp_ai *ai = static_cast< llarp_ai * >(iter->user);
  llarp_ai_copy(&job->ai, ai);
  job->timeout = 5000;
  job->result  = &llarp_router::on_try_connect_result;
  // give router as user pointer
  job->user = router;
  printf("try_establish\n");
  link->try_establish(link, job);
  printf("return true\n");
  return true;
}

extern "C" {

struct llarp_router *
llarp_init_router(struct llarp_alloc *mem, struct llarp_threadpool *tp,
                  struct llarp_ev_loop *netloop, struct llarp_logic *logic)
{
  llarp_router *router = new llarp_router(mem);
  if(router)
  {
    router->netloop = netloop;
    router->tp      = tp;
    router->logic   = logic;
    llarp_crypto_libsodium_init(&router->crypto);
    llarp_msg_muxer_init(&router->muxer);
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
    printf("router not ready\n");
    return false;
  }
  return router->EnsureIdentity();
}

void
llarp_run_router(struct llarp_router *router)
{
  router->Run();
}

bool
llarp_router_try_connect(struct llarp_router *router, struct llarp_rc *remote)
{
  // try first address only
  llarp_ai addr;
  if(llarp_ai_list_index(remote->addrs, 0, &addr))
  {
    printf("try connect to first address\n");
    llarp_router_iterate_links(router,
                               {&addr, &llarp_router::iter_try_connect});
    return true;
  }
  else
    printf("router has no addresses?\n");
  return false;
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
    for(auto &link : (*router)->links)
    {
      link->free_impl(link);
      delete link;
    }
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
      af    = AF_PACKET;
      proto = LLARP_ETH_PROTO;
    }
    else
    {
      af    = AF_INET;
      proto = std::atoi(val);
    }

    struct llarp_link *link = nullptr;
    if(StrEq(section, "iwp-links"))
    {
      link = new llarp_link;
      llarp::Zero(link, sizeof(llarp_link));

      llarp_iwp_args args = {
          .mem          = self->mem,
          .crypto       = &self->crypto,
          .logic        = self->logic,
          .cryptoworker = self->tp,
          .keyfile      = self->transport_keyfile.c_str(),
      };
      iwp_link_init(link, args, &self->muxer);
    }
    else if(StrEq(section, "iwp-connect"))
    {
      std::error_code ec;
      if(fs::exists(val, ec))
        self->connect.try_emplace(key, val);
      else
        printf("cannot read %s\n", val);
      return;
    }
    else if(StrEq(section, "router"))
    {
      if(StrEq(key, "contact-file"))
      {
        self->our_rc_file = val;
        printf("storing signed rc at %s\n", self->our_rc_file.c_str());
      }
      return;
    }
    else
      return;

    if(llarp_link_initialized(link))
    {
      printf("link initialized...");
      if(link->configure(link, self->netloop, key, af, proto))
      {
        llarp_ai ai;
        link->get_our_address(link, &ai);
        llarp::Addr addr = ai;
        printf("configured on %s as %s\n", key, addr.to_string().c_str());
        self->AddLink(link);
        return;
      }
    }
    self->mem->free(self->mem, link);
    printf("failed to configure link for %s\n", key);
  }

}  // namespace llarp
