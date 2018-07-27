#include "config.hpp"
#include <llarp/config.h>
#include <llarp/net.hpp>
#include "fs.hpp"
#include "ini.hpp"
#include "logger.hpp"
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
      connect   = find_section(top, "connect", section_t{});
      netdb     = find_section(top, "netdb", section_t{});
      dns       = find_section(top, "dns", section_t{});
      iwp_links = find_section(top, "bind", section_t{});
      services  = find_section(top, "services", section_t{});
      dns       = find_section(top, "dns", section_t{});
      return true;
    }
    return false;
  };

}  // namespace llarp

extern "C"
{
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
  llarp_config_iter(struct llarp_config *conf,
                    struct llarp_config_iterator *iter)
  {
    iter->conf                                                   = conf;
    std::map< std::string, llarp::Config::section_t & > sections = {
        {"network", conf->impl.network}, {"connect", conf->impl.connect},
        {"bind", conf->impl.iwp_links},  {"netdb", conf->impl.netdb},
        {"dns", conf->impl.dns},         {"services", conf->impl.services}};

    for(const auto item : conf->impl.router)
      iter->visit(iter, "router", item.first.c_str(), item.second.c_str());

    for(const auto section : sections)
      for(const auto item : section.second)
        iter->visit(iter, section.first.c_str(), item.first.c_str(),
                    item.second.c_str());
  }

  bool
  llarp_ensure_config(const char *fname)
  {
    std::error_code ec;
    if(fs::exists(fname, ec))
      return true;
    if(ec)
    {
      llarp::LogError(ec);
      return false;
    }

    std::ofstream f(fname);
    if(!f.is_open())
    {
      llarp::LogError("failed to open ", fname, " for writing");
      return false;
    }

    f << "[netdb]" << std::endl;
    f << "dir=netdb" << std::endl;
    f << "[bind]" << std::endl;

    std::string ifname;

    if(llarp::GetBestNetIF(ifname, AF_INET))
      f << ifname << "=1090" << std::endl;

    llarp::LogInfo("Generated new config ", fname);
    return true;
  }
}