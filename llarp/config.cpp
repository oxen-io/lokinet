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
      api       = find_section(top, "api", section_t{});
      return true;
    }
    return false;
  };

}  // namespace llarp

bool
llarp_ensure_config(const char *fname, const char *basedir, bool overwrite,
                    bool asRouter)
{
  std::error_code ec;
  if(fs::exists(fname, ec) && !overwrite)
  {
    return true;
  }
  if(ec)
  {
    llarp::LogError(ec);
    return false;
  }

  std::string basepath = "";
  if(basedir)
  {
    basepath = basedir;
#ifndef _WIN32
    basepath += "/";
#else
    basepath += "\\";
#endif
  }

  // abort if client.ini already exists
  if(!asRouter)
  {
    if(fs::exists(basepath + "client.ini", ec) && !overwrite)
    {
      llarp::LogError(
          basepath, "client.ini currently exists, please use -f to overwrite");
      return true;
    }
    if(ec)
    {
      llarp::LogError(ec);
      return false;
    }
  }

  // write fname ini
  std::ofstream f(fname);
  if(!f.is_open())
  {
    llarp::LogError("failed to open ", fname, " for writing");
    return false;
  }
  llarp_generic_ensure_config(f, basepath);
  if(asRouter)
  {
    llarp_ensure_router_config(f, basepath);
  }
  else
  {
    llarp_ensure_client_config(f, basepath);
  }
  llarp::LogInfo("Generated new config ", fname);
  return true;
}

void
llarp_generic_ensure_config(std::ofstream &f, std::string basepath)
{
  f << "# this configuration was auto generated with 'sane' defaults"
    << std::endl;
  f << "# change these values as desired" << std::endl;
  f << std::endl << std::endl;
  f << "[router]" << std::endl;
  f << "# number of crypto worker threads " << std::endl;
  f << "threads=4" << std::endl;
  f << "# path to store signed RC" << std::endl;
  f << "contact-file=" << basepath << "self.signed" << std::endl;
  f << "# path to store transport private key" << std::endl;
  f << "transport-privkey=" << basepath << "transport.private" << std::endl;
  f << "# path to store identity signing key" << std::endl;
  f << "ident-privkey=" << basepath << "identity.private" << std::endl;
  f << "# encryption key for onion routing" << std::endl;
  f << "encryption-privkey=" << basepath << "encryption.private" << std::endl;
  f << std::endl;
  f << "# uncomment following line to set router nickname to 'lokinet'"
    << std::endl;
  f << "# nickname=lokinet" << std::endl;
  f << std::endl << std::endl;

  f << "# admin api (disabled by default)" << std::endl;
  f << "[api]" << std::endl;
  f << "enabled=false" << std::endl;
  f << "# authkey=insertpubkey1here" << std::endl;
  f << "# authkey=insertpubkey2here" << std::endl;
  f << "# authkey=insertpubkey3here" << std::endl;
#ifdef _WIN32
  f << "bind=127.0.0.1:1190" << std::endl;
#else
  f << "bind=unix:" << basepath << "api.socket" << std::endl;
#endif
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
}

void
llarp_ensure_router_config(std::ofstream &f, std::string basepath)
{
  f << "# network settings " << std::endl;
  f << "[network]" << std::endl;
  f << "profiles=" << basepath << "profiles.dat" << std::endl;
  f << std::endl;
  f << "# ROUTERS ONLY: publish network interfaces for handling inbound traffic"
    << std::endl;
  f << "[bind]" << std::endl;
  // get ifname
  std::string ifname;
  if(llarp::GetBestNetIF(ifname, AF_INET))
    f << ifname << "=1090" << std::endl;
  else
    f << "# could not autodetect network interface" << std::endl
      << "# eth0=1090" << std::endl;

  f << std::endl;
}

bool
llarp_ensure_client_config(std::ofstream &f, std::string basepath)
{
  f << "#uncomment for a hidden service" << std::endl;
  f << "#[services]" << std::endl;
  f << "#client=" << basepath << "client.ini" << std::endl;
  f << std::endl;

  f << "# network settings " << std::endl;
  f << "[network]" << std::endl;
  f << "profiles=" << basepath << "profiles.dat" << std::endl;
#ifndef __linux__
  f << "# ";
#endif

  /*
  // done with fname.ini
  // start client.ini
  // write fname ini
  std::ofstream clientini_f(basepath + "client.ini");
  if(!f.is_open())
  {
    llarp::LogError("failed to open ", basepath, "client.ini for writing");
    return false;
  }
  clientini_f << "[client-hidden-service-name]" << std::endl;
  clientini_f << "keyfile=client-keyfile.private" << std::endl;
  */

  // pick ip
  std::string ip = llarp::findFreePrivateRange();
  /*
  struct privatesInUse ifsInUse = llarp_getPrivateIfs();
  std::string ip                = "";
  if(!ifsInUse.ten)
  {
    ip = "10.10.0.1/24";
  }
  else if(!ifsInUse.oneSeven)
  {
    ip = "172.16.10.1/24";
  }
  else if(!ifsInUse.oneNine)
  {
    ip = "192.168.10.1/24";
  }
  else
  {
    llarp::LogError(
        "Couldn't easily detect a private range to map lokinet onto");
    return false;
  }
  */
  if(ip == "")
  {
    llarp::LogError(
        "Couldn't easily detect a private range to map lokinet onto");
    return false;
  }
  llarp::LogDebug("Detected " + ip
                  + " is available for use, configuring as such");
  // clientini_f << "ifaddr=" << ip << std::endl;
  // pick interface name
  std::string ifName = llarp::findFreeLokiTunIfName();
  if(ifName == "")
  {
    llarp::LogError("Could not find any free lokitun interface names");
    return false;
  }
  /*
  clientini_f << "ifname=lokinum" << std::to_string(num) << std::endl;
  // prefetch-tags=test
  // enable netns?

  llarp::LogInfo("Generated hidden service client as " + basepath
                 + "client.ini");
  */

  f << "ifname=" << ifName << std::endl;
  f << "ifaddr=" << ip << std::endl;
  f << "enabled=true" << std::endl;

  return true;
}

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
        {"network", conf->impl.network}, {"connect", conf->impl.connect},
        {"system", conf->impl.system},   {"bind", conf->impl.iwp_links},
        {"netdb", conf->impl.netdb},     {"dns", conf->impl.dns},
        {"api", conf->impl.api},         {"services", conf->impl.services}};

    for(const auto item : conf->impl.router)
      iter->visit(iter, "router", item.first.c_str(), item.second.c_str());

    for(const auto &section : sections)
      for(const auto &item : section.second)
        iter->visit(iter, section.first.c_str(), item.first.c_str(),
                    item.second.c_str());
  }
}
