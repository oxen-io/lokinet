#include <llarp/router.h>
#include <llarp/link.h>
#include "link.hpp"
#include "mem.hpp"
#include <list>
#include "str.hpp"

namespace llarp
{
  void router_iter_config(llarp_config_iterator * iter, const char * section, const char * key, const char * val);

  
  struct Router
  {
    std::list<Link *> Links;
    llarp_crypto * crypto;
    void Close()
    {
      if(Links.size())
      {
        for(auto & itr : Links)
        {
          llarp_ev_close_udp_listener(itr->Listener());
        }
        Links.clear();
      }
    }

    bool Configured()
    {
      if(Links.size()) return true;
      return false;
    }
  };

}


extern "C" {

  struct llarp_router
  {
    llarp::Router impl;
    llarp_crypto crypto;
  };
  
  void llarp_init_router(struct llarp_router ** router)
  {
    *router = llarp::alloc<llarp_router>(&llarp_g_mem);
    if(*router)
    {
      llarp_crypto_libsodium_init(&(*router)->crypto);
    }
  }

  int llarp_configure_router(struct llarp_router * router, struct llarp_config * conf)
  {
    llarp_config_iterator iter;
    iter.user = router;
    iter.visit = llarp::router_iter_config;
    llarp_config_iter(conf, &iter);
    return router->impl.Configured() ? 0 : -1;
  }

  void llarp_run_router(struct llarp_router * router, struct llarp_ev_loop * loop)
  {
    if(router->impl.Links.size())
      for(auto & iter : router->impl.Links)
        llarp_ev_add_udp_listener(loop, iter->Listener());
  }

  void llarp_free_router(struct llarp_router ** router)
  {
    if(*router)
    {
      llarp_router * r = *router;
      r->impl.Close();
      llarp_g_mem.free(r);
    }
    *router = nullptr;
  }
}

namespace llarp
{
  
  void router_iter_config(llarp_config_iterator * iter, const char * section, const char * key, const char * val)
  {
    llarp_router * self = static_cast<llarp_router *>(iter->user);
    if (StrEq(section, "links"))
    {
      if(StrEq(val, "ip"))
      {
        self->impl.Links.push_back(new Link(&self->crypto));
      }
      else if (StrEq(val, "eth"))
      {
        /** todo: ethernet link */
      }
    }
  }
}
