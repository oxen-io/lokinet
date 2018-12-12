#include <link/curvecp.hpp>
#include <link/server.hpp>
#include <llarp/messages/link_intro.hpp>

namespace llarp
{
  namespace curvecp
  {
    std::unique_ptr< ILinkLayer >
    NewServer(__attribute__((unused)) llarp::Router* r)
    {
      return nullptr;
    }
  }  // namespace curvecp

}  // namespace llarp
