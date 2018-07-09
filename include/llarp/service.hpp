#ifndef LLARP_SERVICE_HPP
#define LLARP_SERVICE_HPP
#include <iostream>
#include <llarp/service/Identity.hpp>
#include <llarp/service/Intro.hpp>
#include <llarp/service/IntroSet.hpp>
#include <llarp/service/endpoint.hpp>
#include <llarp/service/types.hpp>
#include <set>
#include <string>

namespace llarp
{
  namespace service
  {
    std::string
    AddressToString(const Address& addr);

    struct Config
    {
      typedef std::list< std::pair< std::string, std::string > >
          section_values_t;
      typedef std::pair< std::string, section_values_t > section_t;

      std::list< section_t > services;
      bool
      Load(const std::string& fname);
    };

  };  // namespace service
}  // namespace llarp

#endif