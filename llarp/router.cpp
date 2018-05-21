#include "router.hpp"
#include <llarp/ibfq.h>
#include <llarp/dtls.h>
#include <llarp/iwp.h>
#include <llarp/link.h>
#include <llarp/proto.h>
#include <llarp/router.h>
#include "str.hpp"
#include "buffer.hpp"

#include <fstream>

namespace llarp {
void router_iter_config(llarp_config_iterator *iter, const char *section,
                        const char *key, const char *val);
}  // namespace llarp

llarp_router::llarp_router(struct llarp_alloc *m) : ready(false), mem(m) { llarp_msg_muxer_init(&muxer); }

llarp_router::~llarp_router() {}

void llarp_router::AddLink(struct llarp_link *link) {
  llarp::router_links *head = &links;
  while (head->next && head->link) head = head->next;

  if (head->link)
  {
    void * ptr = mem->alloc(mem, sizeof(llarp::router_links), 8);
    head->next = new (ptr) llarp::router_links{link, nullptr};
  }
  else
    head->link = link;

  ready = true;
}

bool llarp_router::Ready() { return ready; }

bool llarp_findOrCreateIdentity(llarp_crypto *crypto, fs::path path,
                                llarp_seckey_t *identity) {
  std::error_code ec;
  if(!fs::exists(path, ec))
  {
    crypto->keygen(*identity);
    std::ofstream f(path, std::ios::binary);
    if(f.is_open())
    {
      f.write((char*)identity, sizeof(identity));
    }
  }
  std::ifstream f(path, std::ios::binary);
  if(f.is_open())
  {
    f.read((char*)identity, sizeof(identity));
    return true;
  }
  return false;
}

bool llarp_router::EnsureIdentity()
{
  std::error_code ec;
  if(!fs::exists(ident_keyfile, ec))
  {
    crypto.keygen(identity);
    std::ofstream f(ident_keyfile, std::ios::binary);
    if(f.is_open())
    {
      f.write((char*)identity, sizeof(identity));
    }
  }
  std::ifstream f(ident_keyfile, std::ios::binary);
  if(f.is_open())
  {
    f.read((char*)identity, sizeof(identity));
    return true;
  }
  return false;
}

bool llarp_rc_write(struct llarp_rc *rc, fs::path our_rc_file) {
  uint8_t tmp[MAX_RC_SIZE];
  llarp_buffer_t buf;
  buf.base = (char*)tmp;
  buf.cur = (char*) tmp;
  buf.sz = sizeof(tmp);
  if(llarp_rc_bencode(rc, &buf))
  {
    std::ofstream f(our_rc_file, std::ios::binary);
    if(f.is_open())
    {
      f.write(buf.base, buf.cur - buf.base);
      return true;
    }
  }
  return false;
}

bool llarp_router::SaveRC()
{
  printf("verify rc signature... ");
  if(!llarp_rc_verify_sig(&crypto, &rc))
  {
    printf(" BAD!\n");
    return false;
  }
  printf(" OK.\n");

  uint8_t tmp[MAX_RC_SIZE];
  auto buf = llarp::StackBuffer<decltype(tmp)>(tmp);
  
  if(llarp_rc_bencode(&rc, &buf))
  {
    std::ofstream f(our_rc_file, std::ios::binary);
    if(f.is_open())
    {
      f.write(buf.base, buf.cur - buf.base);
      return true;
    }
  }
  return false;
}

void llarp_router::ForEachLink(std::function<void(llarp_link *)> visitor) {
  llarp::router_links *cur = &links;
  do {
    if (cur->link) visitor(cur->link);
    cur = cur->next;
  } while (cur);
}

void llarp_router::Close() {
  ForEachLink([](llarp_link *l) { l->stop_link(l); });
}
extern "C" {

  struct llarp_router *llarp_init_router(struct llarp_alloc * mem, struct llarp_threadpool *tp, struct llarp_ev_loop * netloop, struct llarp_logic * logic) {
  void * ptr = mem->alloc(mem, sizeof(llarp_router), 16);
  if(!ptr) return nullptr;
  llarp_router *router = new (ptr) llarp_router(mem);
  if(router)
  {
    router->netloop = netloop;
    router->tp = tp;
    router->logic = logic;
    llarp_crypto_libsodium_init(&router->crypto);
  }
  return router;
}

bool llarp_configure_router(struct llarp_router *router,
                            struct llarp_config *conf) {
  llarp_config_iterator iter;
  iter.user = router;
  iter.visit = llarp::router_iter_config;
  llarp_config_iter(conf, &iter);
  if(!router->Ready()) return false;
  return router->EnsureIdentity();
}

void llarp_rc_clear(struct llarp_rc *rc) {
  // zero out router contact
  llarp::Zero(rc, sizeof(llarp_rc));
}

bool llarp_rc_addr_list_iter(struct llarp_ai_list_iter *iter,
                             struct llarp_ai *ai) {
  struct llarp_rc *rc = (llarp_rc *)iter->user;
  llarp_ai_list_pushback(rc->addrs, *ai);
  return true;
}

void llarp_rc_set_addrs(struct llarp_rc *rc, struct llarp_alloc *mem,
                        struct llarp_ai_list *addr) {
  rc->addrs = llarp_ai_list_new(mem);
  struct llarp_ai_list_iter ai_itr;
  ai_itr.user = rc;
  ai_itr.visit = &llarp_rc_addr_list_iter;
  llarp_ai_list_iterate(addr, &ai_itr);
}

void llarp_rc_set_pubkey(struct llarp_rc *rc, uint8_t *pubkey) {
  // set public key
  memcpy(rc->pubkey, pubkey, 32);
}

void llarp_rc_sign(llarp_crypto *crypto, llarp_seckey_t *identity,
                   struct llarp_rc *rc) {
  // sign router contact
  llarp_buffer_t signbuf;
  char buf[MAX_RC_SIZE];
  signbuf.base = buf;
  signbuf.cur = buf;
  signbuf.sz = sizeof(buf);
  // encode
  if(llarp_rc_bencode(rc, &signbuf))
  {
    printf("router.cpp::llarp_rc_sign - encoded [%s]\n", buf);
    // sign
    signbuf.sz = signbuf.cur - signbuf.base;
    printf("router.cpp::llarp_rc_sign - sized [%d/%d]\n", signbuf.sz, MAX_RC_SIZE);
    crypto->sign(rc->signature, *identity, signbuf);
    printf("router.cpp::llarp_rc_sign - signed\n");
  }
}

void llarp_run_router(struct llarp_router *router) {

  // zero out router contact
  llarp::Zero(&router->rc, sizeof(llarp_rc));
  // fill our address list
  router->rc.addrs = llarp_ai_list_new(router->mem);
  router->ForEachLink([router](llarp_link *link) {
      llarp_ai addr;
      link->get_our_address(link, &addr);
      llarp_ai_list_pushback(router->rc.addrs, addr);
  });
  // set public key
  memcpy(router->rc.pubkey, router->pubkey(), 32);

  // sign router contact
  char buf[MAX_RC_SIZE];
  auto signbuf = llarp::StackBuffer<decltype(buf)>(buf);
  // encode
  if(llarp_rc_bencode(&router->rc, &signbuf))
  {
    // sign
    signbuf.sz = signbuf.cur - signbuf.base;
    router->crypto.sign(router->rc.signature, router->identity, signbuf);
    if(router->SaveRC())
    {
      printf("saved router contact\n");
      llarp_logic * logic = router->logic;
      router->ForEachLink([logic](llarp_link *link) {
        int result = link->start_link(link, logic);
        if (result == -1) printf("link %s failed to start\n", link->name());
      });
      return;
    }
  }
  printf("failed to generate rc\n");
}

void llarp_stop_router(struct llarp_router *router) {
  if(router)
    router->Close();
}

void llarp_free_router(struct llarp_router **router) {
  if (*router) {
    struct llarp_alloc * mem = (*router)->mem;
    (*router)->ForEachLink([mem](llarp_link *link) { link->free_impl(link); mem->free(mem, link); });
    (*router)->~llarp_router();
    mem->free(mem, *router);
  }
  *router = nullptr;
}
}

namespace llarp
{

void router_iter_config(llarp_config_iterator *iter, const char *section,
                        const char *key, const char *val)
{
  llarp_router *self = static_cast<llarp_router *>(iter->user);
  int af;
  uint16_t proto;
  if (StrEq(val, "eth"))
  {
    af = AF_PACKET;
    proto = LLARP_ETH_PROTO;
  }
  else
  {
    af = AF_INET;
    proto = std::atoi(val);
  }

  struct llarp_link *link = nullptr;
  if (StrEq(section, "iwp-links"))
  {
    link = llarp::Alloc<llarp_link>(self->mem);
    llarp::Zero(link, sizeof(*link));

    llarp_iwp_args args = {
      .mem = self->mem,
      .crypto = &self->crypto,
      .logic = self->logic,
      .cryptoworker = self->tp,
      .keyfile = self->transport_keyfile.c_str(),
    };
    iwp_link_init(link, args, &self->muxer);
  }
  else if (StrEq(section, "iwp-connect"))
  {
    std::error_code ec;
    if(fs::exists(val, ec))
      self->connect.try_emplace(key, val);
    else
      printf("cannot read %s\n", val);
    return;
  }
  else if (StrEq(section, "router"))
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
    if (link->configure(link, self->netloop, key, af, proto))
    {
      printf("configured on %s\n", key);
      self->AddLink(link);
      return;
    }
  }
  self->mem->free(self->mem, link);
  printf("failed to configure link for %s\n", key);
}

}  // namespace llarp
