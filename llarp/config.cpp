#include <config.hpp>

#include <constants/defaults.hpp>
#include <net/net.hpp>
#include <util/fs.hpp>
#include <util/ini.hpp>
#include <util/logger.hpp>
#include <util/mem.hpp>

#include <fstream>
#include <ios>
#include <iostream>

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
                          ret.emplace_back(string_view_string(item.first),
                                           string_view_string(item.second));
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
    {
      return false;
    }
    router    = find_section(parser, "router", section_t{});
    network   = find_section(parser, "network", section_t{});
    connect   = find_section(parser, "connect", section_t{});
    netdb     = find_section(parser, "netdb", section_t{});
    dns       = find_section(parser, "dns", section_t{});
    iwp_links = find_section(parser, "bind", section_t{});
    services  = find_section(parser, "services", section_t{});
    system    = find_section(parser, "system", section_t{});
    metrics   = find_section(parser, "metrics", section_t{});
    api       = find_section(parser, "api", section_t{});
    lokid     = find_section(parser, "lokid", section_t{});
    bootstrap = find_section(parser, "bootstrap", section_t{});
    logging   = find_section(parser, "logging", section_t{});
    return true;
  }

  void
  Config::visit(const Visitor &functor)
  {
    std::unordered_map< std::string, const llarp::Config::section_t & >
        sections = {{"network", network},
                    {"connect", connect},
                    {"bootstrap", bootstrap},
                    {"system", system},
                    {"metrics", metrics},
                    {"netdb", netdb},
                    {"api", api},
                    {"services", services}};

    auto visitor = [&](const char *name, const auto &item) {
      functor(name, item.first.c_str(), item.second.c_str());
    };

    using namespace std::placeholders;

    std::for_each(logging.begin(), logging.end(),
                  std::bind(visitor, "logging", _1));
    // end of logging section commit settings and go
    functor("logging", "", "");
    std::for_each(lokid.begin(), lokid.end(), std::bind(visitor, "lokid", _1));
    std::for_each(router.begin(), router.end(),
                  std::bind(visitor, "router", _1));

    std::for_each(dns.begin(), dns.end(), std::bind(visitor, "dns", _1));
    std::for_each(iwp_links.begin(), iwp_links.end(),
                  std::bind(visitor, "bind", _1));

    std::for_each(sections.begin(), sections.end(), [&](const auto &section) {
      std::for_each(section.second.begin(), section.second.end(),
                    std::bind(visitor, section.first.c_str(), _1));
    });
  }

}  // namespace llarp

/// fname should be a relative path (from CWD) or absolute path to the config
/// file
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

  llarp::LogInfo("Attempting to create config file ", fname);

  // abort if config already exists
  if(!asRouter)
  {
    if(fs::exists(fname, ec) && !overwrite)
    {
      llarp::LogError(fname, " currently exists, please use -f to overwrite");
      return true;
    }
    if(ec)
    {
      llarp::LogError(ec);
      return false;
    }
  }

  // write fname ini
  auto optional_f =
      llarp::util::OpenFileStream< std::ofstream >(fname, std::ios::binary);
  if(!optional_f || !optional_f.value().is_open())
  {
    llarp::LogError("failed to open ", fname, " for writing");
    return false;
  }
  auto &f = optional_f.value();
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
  f << "#nickname=lokinet" << std::endl;
  f << std::endl << std::endl;

  // logging
  f << "[logging]" << std::endl;
  f << "level=info" << std::endl;
  f << "# uncomment for logging to file" << std::endl;
  f << "#type=file" << std::endl;
  f << "#file=/path/to/logfile" << std::endl;
  f << "# uncomment for syslog logging" << std::endl;
  f << "#type=syslog" << std::endl;

  // metrics
  f << "[metrics]" << std::endl;
  f << "json-metrics-path=" << basepath << "metrics.json" << std::endl;

  f << std::endl << std::endl;

  f << "# admin api (disabled by default)" << std::endl;
  f << "[api]" << std::endl;
  f << "enabled=false" << std::endl;
  f << "#authkey=insertpubkey1here" << std::endl;
  f << "#authkey=insertpubkey2here" << std::endl;
  f << "#authkey=insertpubkey3here" << std::endl;
  f << "bind=127.0.0.1:1190" << std::endl;
  f << std::endl << std::endl;

  f << "# system settings for privileges and such" << std::endl;
  f << "[system]" << std::endl;
  f << "user=" << DEFAULT_LOKINET_USER << std::endl;
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
  // we only process one of these...
  // f << "# add another bootstrap node" << std::endl;
  // f << "#add-node=/path/to/alternative/self.signed" << std::endl;
  f << std::endl << std::endl;
}

void
llarp_ensure_router_config(std::ofstream &f, std::string basepath)
{
  f << "# lokid settings (disabled by default)" << std::endl;
  f << "[lokid]" << std::endl;
  f << "enabled=false" << std::endl;
  f << "jsonrpc=127.0.0.1:22023" << std::endl;
  f << "#service-node-seed=/path/to/servicenode/seed" << std::endl;
  f << std::endl;
  f << "# network settings " << std::endl;
  f << "[network]" << std::endl;
  f << "profiles=" << basepath << "profiles.dat" << std::endl;
  // better to let the routers auto-configure
  // f << "ifaddr=auto" << std::endl;
  // f << "ifname=auto" << std::endl;
  f << "enabled=true" << std::endl;
  f << "exit=false" << std::endl;
  f << "#exit-blacklist=tcp:25" << std::endl;
  f << "#exit-whitelist=tcp:*" << std::endl;
  f << "#exit-whitelist=udp:*" << std::endl;
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
      << "#eth0=1090" << std::endl;

  f << std::endl;
}

bool
llarp_ensure_client_config(std::ofstream &f, std::string basepath)
{
  // write snapp-example.ini
  const std::string snappExample_fpath = basepath + "snapp-example.ini";
  {
    auto stream = llarp::util::OpenFileStream< std::ofstream >(
        snappExample_fpath, std::ios::binary);
    if(!stream)
      return false;
    auto &example_f = stream.value();
    if(example_f.is_open())
    {
      // pick ip
      // don't revert me
      const static std::string ip = "10.33.0.1/16";
      /*
      std::string ip = llarp::findFreePrivateRange();
      if(ip == "")
      {
        llarp::LogError(
            "Couldn't easily detect a private range to map lokinet onto");
        return false;
      }
     */
      example_f << "# this is an example configuration for a snapp"
                << std::endl;
      example_f << "[example-snapp]" << std::endl;
      example_f << "# keyfile is the path to the private key of the snapp, "
                   "your .loki is tied to this key, DON'T LOSE IT"
                << std::endl;
      example_f << "keyfile=" << basepath << "example-snap-keyfile.private"
                << std::endl;
      example_f << "# ifaddr is the ip range to allocate to this snapp"
                << std::endl;
      example_f << "ifaddr=" << ip << std::endl;
      // probably fine to leave this (and not-auto-detect it) I'm not worried
      // about any collisions
      example_f << "# ifname is the name to try and give to the network "
                   "interface this snap owns"
                << std::endl;
      example_f << "ifname=snapp-tun0" << std::endl;
    }
    else
    {
      llarp::LogError("failed to write ", snappExample_fpath);
    }
  }
  // now do up fname
  f << std::endl << std::endl;
  f << "# snapps configuration section" << std::endl;
  f << "[services]";
  f << "# uncomment next line to enable a snapp" << std::endl;
  f << "#example-snapp=" << snappExample_fpath << std::endl;
  f << std::endl << std::endl;

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

  // better to set them to auto then to hard code them now
  // operating environment may change over time and this will help adapt
  // f << "ifname=auto" << std::endl;
  // f << "ifaddr=auto" << std::endl;

  // should this also be auto? or not declared?
  // probably auto in case they want to set up a hidden service
  f << "enabled=true" << std::endl;
  return true;
}
