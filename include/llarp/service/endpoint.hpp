#ifndef LLARP_SERVICE_ENDPOINT_HPP
#define LLARP_SERVICE_ENDPOINT_HPP
#include <llarp/pathbuilder.hpp>
#include <llarp/service/Identity.hpp>

namespace llarp
{
  namespace service
  {
    struct Endpoint
    {
      Endpoint(const std::string& nickname, llarp_router* r);
      ~Endpoint();

      bool
      SetOption(const std::string& k, const std::string& v);

      bool
      Start();

     private:
      llarp_router* m_Router;
      llarp_pathbuilder_context* m_PathSet;
      std::string m_Keyfile;
      Identity m_Identity;
    };
  }  // namespace service
}  // namespace llarp

#endif