#ifndef LLARP_SERVICE_CONFIG_HPP
#define LLARP_SERVICE_CONFIG_HPP
#include <list>
#include <string>

namespace llarp
{
  namespace service
  {
    struct Config
    {
      typedef std::list< std::pair< std::string, std::string > >
          section_values_t;
      typedef std::pair< std::string, section_values_t > section_t;

      std::list< section_t > services;

      bool
      Load(const std::string& fname);
    };
  }  // namespace service
}  // namespace llarp
#endif