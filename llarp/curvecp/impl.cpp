#include <llarp/link/curvecp.hpp>
#include "router.hpp"

namespace llarp
{
  namespace curvecp
  {
    struct LinkLayer : public llarp::ILinkLayer
    {
      LinkLayer(llarp_router* r) : llarp::ILinkLayer(r)
      {
      }

      ~LinkLayer()
      {
      }

      const char*
      Name() const
      {
        return "curvecp";
      }
    };

    std::unique_ptr< llarp::ILinkLayer >
    NewServer(llarp_router* r)
    {
      return std::unique_ptr< llarp::ILinkLayer >(new LinkLayer(r));
    }
  }  // namespace curvecp

}  // namespace llarp
