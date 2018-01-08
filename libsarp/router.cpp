#include <sarp/router.h>
#include <sarp/link.h>
#include "link.hpp"
#include <vector>

namespace sarp
{
  struct Router
  {
    std::vector<Link> Links;

    static void iter_config(sarp_config_iterator * iter, const char * section, const char * key, const char * val)
    {
      sarp_router * self = static_cast<sarp_router *>(iter->user);
    }



    bool Configured()
    {
      return false;
    }
  };

}

extern "C" {

  struct sarp_router
  {
    sarp::Router impl;
  };
  
  void sarp_init_router(struct sarp_router ** router)
  {
    *router = static_cast<sarp_router *>(sarp_g_mem.malloc(sizeof(sarp_router)));
  }

  int sarp_configure_router(struct sarp_router * router, struct sarp_config * conf)
  {
    sarp_config_iterator iter;
    iter.user = router;
    iter.visit = sarp::Router::iter_config;
    sarp_config_iter(conf, iter);
    return router->impl.Configured() ? 0 : -1;
  }

  void sarp_run_router(struct sarp_router * router, struct sarp_ev_loop * loop)
  {
    for(auto & iter : router->impl.Links)
      sarp_ev_add_udp_listener(loop, &iter.listener);
  }

  void sarp_free_router(struct sarp_router ** router)
  {
    if(*router)
      sarp_g_mem.free(*router);
    *router = nullptr;
  }
}
