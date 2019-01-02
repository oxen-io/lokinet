#ifndef LLARP_LINK_DTLS_HPP
#define LLARP_LINK_DTLS_HPP

#include <memory>

namespace llarp
{
  struct ILinkLayer;
  struct Router;

  namespace dtls
  {
    std::unique_ptr< ILinkLayer >
    NewServerFromRouter(llarp::Router* r);
  }
}  // namespace llarp

#endif
