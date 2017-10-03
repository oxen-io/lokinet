#include <sarp/config.h>
#include "ini.hpp"

namespace sarp
{
  struct config
  {
    typedef std::map<std::string, std::string> section_t;
    section_t router;
    section_t network;
    section_t netdb;

    bool Load(const char * fname)
    {
      std::ifstream f;
      f.open(fname);
      if(f.is_open())
      {
        ini::Parser parser(f);
        auto & top = parser.top();
        
        return true;
      }
      return false;
    };
    
  };
}


extern "C" {
  struct sarp_config
  {
    sarp::config impl;
    sarp_alloc * mem;
  };

  void sarp_new_config(struct sarp_config ** conf, struct sarp_alloc * mem)
  {
    sarp_config * c = static_cast<sarp_config*>(mem->malloc(sizeof(struct sarp_config)));
    c->mem = mem;
    *conf = c;
  }

  void sarp_free_config(struct sarp_config ** conf)
  {
    sarp_alloc * mem = (*conf)->mem;
    mem->free(*conf);
    *conf = nullptr;
  }

  int sarp_load_config(struct sarp_config * conf, const char * fname)
  {
    if(!conf->impl.Load(fname)) return -1;
    return 0;
  }
}
