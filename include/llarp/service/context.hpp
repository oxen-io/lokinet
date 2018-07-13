#ifndef LLARP_SERVICE_CONTEXT_HPP
#define LLARP_SERVICE_CONTEXT_HPP
#include <llarp/router.h>
#include <llarp/service/config.hpp>
#include <llarp/service/endpoint.hpp>
#include <unordered_map>

namespace llarp
{
  namespace service
  {
    /// holds all the hidden service endpoints we own
    struct Context
    {
      Context(llarp_router *r);
      ~Context();

      void
      Tick();

      bool
      AddEndpoint(const Config::section_t &conf);

     private:
      llarp_router *m_Router;
      std::unordered_map< std::string, Endpoint * > m_Endpoints;
    };
  }
}
#endif