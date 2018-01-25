#include <sarp/config.h>
#include <sarp/mem.h>
#include "config.hpp"
#include "ini.hpp"

namespace sarp
{

  template<typename Config, typename Section>
  static Section find_section(Config & c, const std::string & name, const Section & fallback)
  {
    if(c.sections.find(name) == c.sections.end())
      return fallback;
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
      links = find_section(top, "links", section_t{});
      return true;
    }
    return false;
  };

  
}


extern "C" {


  
  void sarp_new_config(struct sarp_config ** conf)
  {
    sarp_config * c = static_cast<sarp_config *>(sarp_g_mem.malloc(sizeof(sarp_config)));
    *conf = c;
  }

  void sarp_free_config(struct sarp_config ** conf)
  {
    if(*conf)
      sarp_g_mem.free(*conf);
    *conf = nullptr;
  }

  int sarp_load_config(struct sarp_config * conf, const char * fname)
  {
    if(!conf->impl.Load(fname)) return -1;
    return 0;
  }

  void sarp_config_iter(struct sarp_config * conf, struct sarp_config_iterator * iter)
  {
    iter->conf = conf;
    std::map<std::string, sarp::Config::section_t&> sections = {
      {"router", conf->impl.router},
      {"network", conf->impl.network},
      {"links", conf->impl.links},
      {"netdb", conf->impl.netdb}
    };
    for(const auto & section : sections)
      for(const auto & item : section.second)
        iter->visit(iter, section.first.c_str(), item.first.c_str(), item.second.c_str());
  }
}
