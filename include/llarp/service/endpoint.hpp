#ifndef LLARP_SERVICE_ENDPOINT_HPP
#define LLARP_SERVICE_ENDPOINT_HPP
#include <llarp/pathbuilder.hpp>
#include <llarp/service/Identity.hpp>

namespace llarp
{
  namespace service
  {
    struct Endpoint : public llarp_pathbuilder_context
    {
      Endpoint(const std::string& nickname, llarp_router* r);
      ~Endpoint();

      bool
      SetOption(const std::string& k, const std::string& v);

      void
      Tick();

      bool
      Start();

      bool
      HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg);

     private:
      llarp_router* m_Router;
      std::string m_Keyfile;
      std::string m_Name;
      Identity m_Identity;
    };
  }  // namespace service
}  // namespace llarp

#endif