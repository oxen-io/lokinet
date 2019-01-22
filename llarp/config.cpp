#include <config.h>
#include <config.hpp>

#include <constants/defaults.hpp>
#include <net/net.hpp>
#include <util/fs.hpp>
#include <util/ini.hpp>
#include <util/logger.hpp>
#include <util/mem.hpp>

namespace llarp
{
  template < typename Config, typename Section >
  Section
  find_section(Config &c, const std::string &name, const Section &fallback)
  {
    Section ret;
    if(c.VisitSection(name.c_str(),
                      [&ret](const ConfigParser::Section_t &s) -> bool {
                        for(const auto &item : s)
                        {
                          ret.emplace_back(item.first, item.second);
                        }
                        return true;
                      }))
      return ret;
    else
      return fallback;
  }

  bool
  Config::Load(const char *fname)
  {
    ConfigParser parser;
    if(!parser.LoadFile(fname))
      return false;
    router    = find_section(parser, "router", section_t{});
    network   = find_section(parser, "network", section_t{});
    connect   = find_section(parser, "connect", section_t{});
    netdb     = find_section(parser, "netdb", section_t{});
    dns       = find_section(parser, "dns", section_t{});
    iwp_links = find_section(parser, "bind", section_t{});
    services  = find_section(parser, "services", section_t{});
    system    = find_section(parser, "system", section_t{});
    api       = find_section(parser, "api", section_t{});
    lokid     = find_section(parser, "lokid", section_t{});
    bootstrap = find_section(parser, "bootstrap", section_t{});
    return true;
  };

}  // namespace llarp

extern "C" bool
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
  f << "bind=127.0.0.1:1190" << std::endl;
  f << std::endl << std::endl;

  f << "# system settings for privileges and such" << std::endl;
  f << "[system]" << std::endl;
#ifdef _WIN32
  f << "# ";
#endif
  f << "user=" << DEFAULT_LOKINET_USER << std::endl;
#ifdef _WIN32
  f << "# ";
#endif
  f << "group=" << DEFAULT_LOKINET_GROUP << std::endl;
  f << "pidfile=" << basepath << "lokinet.pid" << std::endl;
  f << std::endl << std::endl;

  f << "# dns provider configuration section" << std::endl;
  f << "[dns]" << std::endl;
  f << "# resolver" << std::endl;
  f << "upstream=" << DEFAULT_RESOLVER_US << std::endl;

// Make auto-config smarter
// will this break reproducibility rules?
// (probably)
#ifdef __linux__
#ifdef ANDROID
  f << "bind=127.0.0.1:1153" << std::endl;
#else
  f << "bind=127.3.2.1:53" << std::endl;
#endif
#else
  f << "bind=127.0.0.1:53" << std::endl;
#endif
  f << std::endl << std::endl;

  f << "# network database settings block " << std::endl;
  f << "[netdb]" << std::endl;
  f << "# directory for network database skiplist storage" << std::endl;
  f << "dir=" << basepath << "netdb" << std::endl;
  f << std::endl << std::endl;

  f << "# bootstrap settings" << std::endl;
  f << "[bootstrap]" << std::endl;
  f << "# add a bootstrap node's signed identity to the list of nodes we want "
       "to bootstrap from"
    << std::endl;
  f << "# if we don't have any peers we connect to this router" << std::endl;
  f << "add-node=" << basepath << "bootstrap.signed" << std::endl;
  f << "# add another bootstrap node" << std::endl;
  f << "#add-node=/path/to/alternative/self.signed" << std::endl;
  f << std::endl << std::endl;
}

void
llarp_ensure_router_config(std::ofstream &f, std::string basepath)
{
  f << "# lokid settings (disabled by default)" << std::endl;
  f << "[lokid]" << std::endl;
  f << "enabled=false" << std::endl;
  f << "jsonrpc=127.0.0.1:22023" << std::endl;
  f << "#service-node-seed=/path/to/servicenode/seed" << std::Endl;
  f << std::endl;
  f << "# network settings " << std::endl;
  f << "[network]" << std::endl;
  f << "profiles=" << basepath << "profiles.dat" << std::endl;
  f << "ifaddr=10.105.0.1/16" << std::endl;
  f << "ifname=lokitun0" << std::endl;
  f << "enabled=true" << std::endl;
  f << "exit=false" << std::endl;
  f << "# exit-blacklist=tcp:25" << std::endl;
  f << "# exit-whitelist=tcp:*" << std::endl;
  f << "# exit-whitelist=udp:*" << std::endl;
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
#ifndef _WIN32
  const std::string snappExample_fpath = basepath + "snapp-example.ini";
  // done with fname.ini
  // start client.ini
  // write fname ini
  {
    std::ofstream clientini_f(snappExample_fpath);
    if(f.is_open())
    {
      clientini_f << "# this is an example configuration for a snapp";
      clientini_f << "[example-snapp]" << std::endl;
      clientini_f << "# keyfile is the path to the private key of the snapp"
                  << std::endl;
      clientini_f << "keyfile=" << basepath << "example-snap-keyfile.private"
                  << std::endl;
      clientini_f << "# ifaddr is the ip range to allocate to this snapp"
                  << std::endl;
      clientini_f << "ifaddr=10.55.0.0/16" << std::endl;
      clientini_f << "# ifname is the name to try and give to the network "
                     "interface this snap owns"
                  << std::endl;
      clientini_f << "ifname=snapp-tun0" << std::endl;
    }
    else
    {
      llarp::LogError("failed to write ", snappExample_fpath);
    }
  }
  f << std::endl << std::endl;
  f << "# snapps configuration section" << std::endl;
  f << "[services]";
  f << "# uncomment next line to enable persistant snapp" << std::endl;
  f << "#example-snapp=" << snappExample_fpath << std::endl;
  f << std::endl << std::endl;
#endif

  f << "# network settings " << std::endl;
  f << "[network]" << std::endl;
  f << "profiles=" << basepath << "profiles.dat" << std::endl;
  f << "# uncomment next line to add router with pubkey to list of routers we "
       "connect directly to"
    << std::endl;
  f << "#strict-connect=pubkey" << std::endl;
  f << "# uncomment next line to use router with pubkey as an exit node"
    << std::endl;
  f << "#exit-node=pubkey" << std::endl;
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
    iter->conf = conf;
    std::unordered_map< std::string, const llarp::Config::section_t & >
        sections = {{"network", conf->impl.network},
                    {"connect", conf->impl.connect},
                    {"bootstrap", conf->impl.bootstrap},
                    {"system", conf->impl.system},
                    {"netdb", conf->impl.netdb},
                    {"api", conf->impl.api},
                    {"services", conf->impl.services}};

    for(const auto &item : conf->impl.lokid)
      iter->visit(iter, "lokid", item.first.c_str(), item.second.c_str());

    for(const auto &item : conf->impl.router)
      iter->visit(iter, "router", item.first.c_str(), item.second.c_str());

    for(const auto &item : conf->impl.dns)
      iter->visit(iter, "dns", item.first.c_str(), item.second.c_str());

    for(const auto &item : conf->impl.iwp_links)
      iter->visit(iter, "bind", item.first.c_str(), item.second.c_str());

    for(const auto &section : sections)
      for(const auto &item : section.second)
        iter->visit(iter, section.first.c_str(), item.first.c_str(),
                    item.second.c_str());
  }
}
