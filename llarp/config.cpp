#include "config.hpp"
#include <llarp/config.h>
#include <llarp/dns.h>
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
    f << "# this configuration was auto generated with 'sane' defaults"
      << std::endl;
    f << "# change these values as desired" << std::endl;
    f << std::endl;
    f << "# configuration for lokinet network interface" << std::endl;
    f << "[network]" << std::endl;
    f << "# interface name" << std::endl;
#ifdef _WIN32
    // comment out ifname section for windows
    f << "# ";
#endif
    f << "ifname=lokitun0" << std::endl;

    f << "# ip range for interface" << std::endl;
    f << "addr=10.0.0.1/16" << std::endl;
    f << std::endl;

    f << "# dns provider configuration section" << std::endl;
    f << "[dns]" << std::endl;
    f << "# opennic us resolver" << std::endl;
    f << "upstream=" << DEFAULT_RESOLVER_US << std::endl;
    f << "# opennic eu resolver" << std::endl;
    f << "upstream=" << DEFAULT_RESOLVER_EU << std::endl;
    f << "# opennic au resolver" << std::endl;
    f << "upstream=" << DEFAULT_RESOLVER_AU << std::endl;
    f << "bind=127.3.2.1:53" << std::endl;
    f << std::endl;

    f << "[netdb]" << std::endl;
    f << "dir=netdb" << std::endl;
    f << std::endl;
    f << "# publish network interfaces for handling inbound traffic"
      << std::endl;
    f << "[bind]" << std::endl;

    std::string ifname;

    if(llarp::GetBestNetIF(ifname, AF_INET))
      f << ifname << "=1090" << std::endl;
    else
      f << "# could not autodetect network interface" << std::endl
        << "# eth0=1090" << std::endl;

    f << std::endl;
    llarp::LogInfo("Generated new config ", fname);
    return true;
  }
}