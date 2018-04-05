#include "router.hpp"
#include <llarp/ibfq.h>
#include <llarp/link.h>
#include <llarp/router.h>
#include <llarp/iwp.h>
#include <llarp/proto.h>
#include "str.hpp"

namespace llarp {
void router_iter_config(llarp_config_iterator *iter, const char *section,
                        const char *key, const char *val);
}  // namespace llarp

llarp_router::llarp_router() : ready(false) { llarp_msg_muxer_init(&muxer); }

llarp_router::~llarp_router() {}

void llarp_router::AddLink(struct llarp_link *link) {
  llarp::router_links *head = &links;
  while (head->next && head->link) head = head->next;

  if (head->link)
    head->next = new llarp::router_links{link, nullptr};
  else
    head->link = link;

  ready = true;
}

bool llarp_router::Ready()
{
  return ready;
}

void llarp_router::ForEachLink(std::function<void(llarp_link *)> visitor) {
  llarp::router_links *cur = &links;
  do {
    if (cur->link) visitor(cur->link);
    cur = cur->next;
  } while (cur);
}

void llarp_router::Close() { ForEachLink([](llarp_link * l) { l->stop_link(l); }); }
extern "C" {

struct llarp_router *llarp_init_router(struct llarp_threadpool *tp) {
  llarp_router *router = new llarp_router;
  router->tp = tp;
  llarp_crypto_libsodium_init(&router->crypto);
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

void llarp_run_router(struct llarp_router *router, struct llarp_ev_loop *loop) {
  router->ForEachLink([loop](llarp_link *link) {
      int result = link->start_link(link, loop);
      if(result == -1)
        printf("link %s failed to start\n", link->name(link));
  });
}

void llarp_stop_router(struct llarp_router *router) { router->Close(); }

void llarp_free_router(struct llarp_router **router) {
  if (*router) {
    (*router)->ForEachLink([](llarp_link *link) { link->free(link); });
    delete *router;
  }
  *router = nullptr;
}
}

namespace llarp {

void router_iter_config(llarp_config_iterator *iter, const char *section,
                        const char *key, const char *val) {
  llarp_router *self = static_cast<llarp_router *>(iter->user);
  if (StrEq(section, "links")) {
    if (StrEq(val, "eth")) {
      struct llarp_link *link = llarp::Alloc<llarp_link>();
      iwp_configure_args args = {
        .crypto = &self->crypto
      };
      iwp_link_init(link, args, &self->muxer);
      if (link->configure(link, key, AF_PACKET, LLARP_ETH_PROTO))
      {
        printf("ethernet link configured on %s\n", key);
        self->AddLink(link);
      }
      else {
        link->free(link);
        printf("failed to configure ethernet link for %s\n", key);
      }
    } else {
      struct llarp_link *link = llarp::Alloc<llarp_link>();
      uint16_t port = std::atoi(val);
      iwp_configure_args args = {
        .crypto = &self->crypto
      };
      iwp_link_init(link, args, &self->muxer);
      if (link->configure(link, key, AF_INET6, port))
      {
        printf("inet link configured on %s port %d\n", key, port);
        self->AddLink(link);
      }
      else {
        link->free(link);
        printf("failed to configure inet link for %s port %d\n", key, port);
      }
    } 
  }
}
}  // namespace llarp
