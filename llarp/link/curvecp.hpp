#ifndef LLARP_LINK_CURVECP_HPP
#define LLARP_LINK_CURVECP_HPP

#include <memory>

namespace llarp
{
  struct ILinkLayer;
  struct Router;

  namespace curvecp
  {
    std::unique_ptr< ILinkLayer >
    NewServer(llarp::Router* r);
  }
}  // namespace llarp

#endif
