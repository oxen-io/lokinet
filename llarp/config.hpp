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
    section_t iwp_links;
    section_t connect;

    bool
    Load(const char *fname);
  };
}  // namespace llarp

struct llarp_config
{
  llarp::Config impl;
};

#endif
