#include "router.hpp"
#include <llarp/ibfq.h>
#include <llarp/dtls.h>
#include <llarp/iwp.h>
#include <llarp/link.h>
#include <llarp/proto.h>
#include <llarp/router.h>
#include "str.hpp"

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

struct llarp_router *llarp_init_router(struct llarp_alloc * mem, struct llarp_threadpool *tp, struct llarp_ev_loop * netloop) {
  void * ptr = mem->alloc(mem, sizeof(llarp_router), 16);
  if(!ptr) return nullptr;
  llarp_router *router = new (ptr) llarp_router(mem);
  if(router)
  {
    router->netloop = netloop;
    router->tp = tp;
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
  return router->Ready();
}

void llarp_run_router(struct llarp_router *router, struct llarp_logic *logic) {
  router->ForEachLink([logic](llarp_link *link) {
    int result = link->start_link(link, logic);
    if (result == -1) printf("link %s failed to start\n", link->name());
  });
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

  struct llarp_link *link;
  if (StrEq(section, "dtls-links"))
  {
    link = llarp::Alloc<llarp_link>(self->mem);  
    llarp::Zero(link, sizeof(*link));
    llarp_dtls_args args = {
      .mem = self->mem,
      .keyfile=self->transport_keyfile,
      .certfile=self->transport_certfile,
    };
    dtls_link_init(link, args, &self->muxer);
  }
  else if (StrEq(section, "iwp-links"))
  {
    link = llarp::Alloc<llarp_link>(self->mem);  
    llarp::Zero(link, sizeof(*link));
    
    iwp_configure_args args = {
      .mem = self->mem,
      .crypto = &self->crypto,
      .keyfile=self->transport_keyfile,
    };
    iwp_link_init(link, args, &self->muxer);
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
