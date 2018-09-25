#include "config.hpp"
#include <llarp/config.h>
#include <llarp/defaults.h>
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
      system    = find_section(top, "system", section_t{});
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
    llarp_config *c = new llarp_config();
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
        {"network", conf->impl.network},  {"connect", conf->impl.connect},
        {"system", conf->impl.system},    {"bind", conf->impl.iwp_links},
        {"netdb", conf->impl.netdb},      {"dns", conf->impl.dns},
        {"services", conf->impl.services}};

    for(const auto item : conf->impl.router)
      iter->visit(iter, "router", item.first.c_str(), item.second.c_str());

    for(const auto section : sections)
      for(const auto item : section.second)
        iter->visit(iter, section.first.c_str(), item.first.c_str(),
                    item.second.c_str());
  }

  bool
  llarp_ensure_config(const char *fname, const char *basedir, bool overwrite)
  {
    std::error_code ec;
    if(fs::exists(fname, ec) && !overwrite)
      return true;
    if(ec)
    {
      llarp::LogError(ec);
      return false;
    }
    std::string basepath = "";
    if(basedir)
    {
      basepath = basedir;
      basepath += "/";
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
    f << std::endl << std::endl;

    f << "# router settings block" << std::endl;
    f << "[router]" << std::endl;
    f << "# uncomment these to manually set public address and port"
      << std::endl;
    f << "# this is required on providers like AWS because of their firewall "
         "rules"
      << std::endl;
    f << "# public-address=your.ip.goes.here" << std::endl;
    f << "# public-port=1090" << std::endl;
    f << std::endl;
    f << "# number of crypto worker threads " << std::endl;
    f << "threads=4" << std::endl;
    f << "# path to store signed RC" << std::endl;
    f << "contact-file=" << basepath << "self.signed" << std::endl;
    f << "# path to store transport private key" << std::endl;
    f << "transport-privkey=" << basepath << "transport.private" << std::endl;
    f << "# path to store identity signing key" << std::endl;
    f << "identity-privkey=" << basepath << "identity.private" << std::endl;
    f << std::endl;
    f << "# uncomment following line to set router nickname to 'lokinet'"
      << std::endl;
    f << "# nickname=lokinet" << std::endl;
    f << std::endl << std::endl;

    f << "# system settings for priviledges and such" << std::endl;
    f << "[system]" << std::endl;
#ifdef _WIN32
    f << "# ";
#endif
    f << "user=" << DEFAULT_LOKINET_USER << std::endl;
#ifdef _WIN32
    f << "# ";
#endif
    f << "group=" << DEFAULT_LOKINET_GROUP << std::endl;
    f << std::endl << std::endl;

    f << "# dns provider configuration section" << std::endl;
    f << "[dns]" << std::endl;
    f << "# opennic us resolver" << std::endl;
    f << "upstream=" << DEFAULT_RESOLVER_US << std::endl;
    f << "# opennic eu resolver" << std::endl;
    f << "upstream=" << DEFAULT_RESOLVER_EU << std::endl;
    f << "# opennic au resolver" << std::endl;
    f << "upstream=" << DEFAULT_RESOLVER_AU << std::endl;
    f << "bind=127.3.2.1:53" << std::endl;
    f << std::endl << std::endl;

    f << "# network database settings block " << std::endl;
    f << "[netdb]" << std::endl;
    f << "# directory for network database skiplist storage" << std::endl;
    f << "dir=" << basepath << "netdb" << std::endl;
    f << std::endl << std::endl;

    f << "# bootstrap settings " << std::endl;
    f << "[connect]" << std::endl;
    f << "bootstrap=" << basepath << "bootstrap.signed" << std::endl;
    f << std::endl << std::endl;

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
