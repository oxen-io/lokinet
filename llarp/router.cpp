#include "router.hpp"
#include <llarp/ibfq.h>
#include <llarp/link.h>
#include <llarp/router.h>
#include "link.hpp"
#include "str.hpp"

namespace llarp {
void router_iter_config(llarp_config_iterator *iter, const char *section,
                        const char *key, const char *val);
}  // namespace llarp

llarp_router::llarp_router() { llarp_msg_muxer_init(&muxer); }

llarp_router::~llarp_router() {}

void llarp_router::AddLink(struct llarp_link *link) {
  llarp::router_links *head = &links;
  while (head->next && head->link) head = head->next;

  if (head->link)
    head->next = new llarp::router_links{link, nullptr};
  else
    head->link = link;
}

void llarp_router::ForEachLink(std::function<void(llarp_link *)> visitor) {
  llarp::router_links *cur = &links;
  do {
    if (cur->link) visitor(cur->link);
    cur = cur->next;
  } while (cur);
}

void llarp_router::Close() { ForEachLink(llarp_link_stop); }

extern "C" {

struct llarp_router *llarp_init_router(struct llarp_threadpool *tp) {
  llarp_router *router = new llarp_router;
  router->tp = tp;
  llarp_crypto_libsodium_init(&router->crypto);
  return router;
}

int llarp_configure_router(struct llarp_router *router,
                           struct llarp_config *conf) {
  llarp_config_iterator iter;
  iter.user = router;
  iter.visit = llarp::router_iter_config;
  llarp_config_iter(conf, &iter);
  return 0;
}

void llarp_run_router(struct llarp_router *router, struct llarp_ev_loop *loop) {
  router->ForEachLink([loop](llarp_link *link) {
    llarp_ev_add_udp_listener(loop, llarp_link_udp_listener(link));
  });
}

void llarp_stop_router(struct llarp_router *router) { router->Close(); }

void llarp_free_router(struct llarp_router **router) {
  if (*router) {
    (*router)->ForEachLink([](llarp_link *link) { llarp_g_mem.free(link); });
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
    if (StrEq(val, "ip")) {
      struct llarp_link *link = llarp_link_alloc(&self->muxer);
      if (llarp_link_configure_addr(link, key, AF_INET6, 7000))
        self->AddLink(link);
      else {
        llarp_link_free(&link);
        printf("failed to configure %s link for %s\n", val, key);
      }
    } else if (StrEq(val, "eth")) {
      /** todo: ethernet link */
    }
  }
}
}  // namespace llarp
