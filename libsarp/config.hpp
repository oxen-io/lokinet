#ifndef LIBSARP_CONFIG_HPP
#define LIBSARP_CONFIG_HPP
#include <map>
#include <string>

#include <sarp/config.h>

namespace sarp
{
  struct Config
  {
    typedef std::map<std::string, std::string> section_t;
 
    section_t router;
    section_t network;
    section_t netdb;

    bool Load(const char * fname);
    
  };
}


extern "C" {
  struct sarp_config
  {
    sarp::Config impl;
    sarp_alloc * mem;
  };
}

#endif
