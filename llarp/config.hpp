#ifndef LIBLLARP_CONFIG_HPP
#define LIBLLARP_CONFIG_HPP
#include <list>
#include <string>

#include <llarp/config.h>

namespace llarp
{
  struct Config
  {
    typedef std::list< std::pair< std::string, std::string > > section_t;

    section_t router;
    section_t network;
    section_t netdb;
    section_t dns;
    section_t iwp_links;
    section_t connect;
    section_t services;
    section_t system;
    section_t api;

    bool
    Load(const char *fname);
  };

}  // namespace llarp

struct llarp_config
{
  llarp::Config impl;
};

void
llarp_generic_ensure_config(std::ofstream &f, std::string basepath);

void
llarp_ensure_router_config(std::ofstream &f, std::string basepath);

bool
llarp_ensure_client_config(std::ofstream &f, std::string basepath);

#endif
