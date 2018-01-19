#include <sarp/router.h>
#include <sarp/link.h>
#include "link.hpp"
#include <list>
#include "str.hpp"

namespace sarp
{
  void router_iter_config(sarp_config_iterator * iter, const char * section, const char * key, const char * val);

  
  struct Router
  {
    std::list<Link_ptr> Links;
    sarp_crypto * crypto;
    void Close()
    {
      for(auto & itr : Links)
      {
        sarp_ev_close_udp_listener(&itr->listener);
      }
      Links.clear();
    }

    bool Configured()
    {
      if(Links.size()) return true;
      return false;
    }
  };

}


extern "C" {

  struct sarp_router
  {
    sarp::Router impl;
    sarp_crypto crypto;
  };
  
  void sarp_init_router(struct sarp_router ** router)
  {
    *router = static_cast<sarp_router *>(sarp_g_mem.malloc(sizeof(sarp_router)));
    if(*router)
    {
      sarp_crypto_libsodium_init(&(*router)->crypto);
    }
  }

  int sarp_configure_router(struct sarp_router * router, struct sarp_config * conf)
  {
    sarp_config_iterator iter;
    iter.user = router;
    iter.visit = sarp::router_iter_config;
    sarp_config_iter(conf, iter);
    return router->impl.Configured() ? 0 : -1;
  }

  void sarp_run_router(struct sarp_router * router, struct sarp_ev_loop * loop)
  {
    for(auto & iter : router->impl.Links)
      sarp_ev_add_udp_listener(loop, &iter->listener);
  }

  void sarp_free_router(struct sarp_router ** router)
  {
    if(*router)
    {
      sarp_router * r = *router;
      r->impl.Close();
      sarp_g_mem.free(*router);
    }
    *router = nullptr;
  }
}

namespace sarp
{
  
  void router_iter_config(sarp_config_iterator * iter, const char * section, const char * key, const char * val)
  {
    sarp_router * self = static_cast<sarp_router *>(iter->user);
    if (streq(section, "links"))
    {
      if(streq(val, "ip"))
      {
        self->impl.Links.push_back(std::make_unique<Link>(&self->crypto));
      }
      else if (streq(val, "eth"))
      {
        /** todo: ethernet link */
      }
    }
  }
}
