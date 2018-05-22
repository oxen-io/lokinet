#include "config.hpp"
#include <llarp/config.h>
#include "ini.hpp"
#include "mem.hpp"

namespace llarp
{
  template < typename Config, typename Section >
  static const Section &
  find_section(Config &c, const std::string &name, const Section &fallback)
  {
    if(c.sections.find(name) == c.sections.end())
      return fallback;
    return c.sections[name].values;
  }

  bool
  Config::Load(const char *fname)
  {
    std::ifstream f;
    f.open(fname);
    if(f.is_open())
    {
      ini::Parser parser(f);
      auto &top = parser.top();
      router    = find_section(top, "router", section_t{});
      network   = find_section(top, "network", section_t{});
      connect   = find_section(top, "iwp-connect", section_t{});
      netdb     = find_section(top, "netdb", section_t{});
      iwp_links = find_section(top, "iwp-links", section_t{});
      return true;
    }
    return false;
  };

}  // namespace llarp

extern "C" {

void
llarp_new_config(struct llarp_config **conf)
{
  llarp_config *c = new llarp_config;
  *conf           = c;
}

void
llarp_free_config(struct llarp_config **conf)
{
  if(*conf)
    delete *conf;
  *conf = nullptr;
}

int
llarp_load_config(struct llarp_config *conf, const char *fname)
{
  if(!conf->impl.Load(fname))
    return -1;
  return 0;
}

void
llarp_config_iter(struct llarp_config *conf, struct llarp_config_iterator *iter)
{
  iter->conf                                                   = conf;
  std::map< std::string, llarp::Config::section_t & > sections = {
      {"router", conf->impl.router},
      {"network", conf->impl.network},
      {"iwp-connect", conf->impl.connect},
      {"iwp-links", conf->impl.iwp_links},
      {"netdb", conf->impl.netdb}};
  for(const auto section : sections)
    for(const auto item : section.second)
      iter->visit(iter, section.first.c_str(), item.first.c_str(),
                  item.second.c_str());
}
}
