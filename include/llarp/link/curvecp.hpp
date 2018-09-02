#ifndef LLARP_LINK_CURVECP_HPP
#define LLARP_LINK_CURVECP_HPP

#include <llarp/link_layer.hpp>

namespace llarp
{
  namespace curvecp
  {
    std::unique_ptr< ILinkLayer >
    NewServer(llarp_router* r);
  }
}  // namespace llarp

#endif
