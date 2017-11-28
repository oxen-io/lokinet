#include <sarp/config.h>
#include "config.hpp"
#include "ini.hpp"

namespace sarp
{

  template<typename Config, typename Section>
  static Section find_section(Config & c, const std::string & name, const Section & sect)
  {
    if(c.sections.find(name) == c.sections.end())
      return sect;
    return c.sections[name].values;
  }
   
  
  bool Config::Load(const char * fname)  
  {
    std::ifstream f;
    f.open(fname);
    if(f.is_open())
    {
      ini::Parser parser(f);
      auto & top = parser.top();
      router = find_section(top, "router", section_t{});
      network = find_section(top, "network", section_t{});
      netdb = find_section(top, "netdb", section_t{});
      return true;
    }
    return false;
  };
}


extern "C" {

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
