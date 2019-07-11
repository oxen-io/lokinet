#ifndef LLARP_UTP_UTP_HPP
#define LLARP_UTP_UTP_HPP

#include <memory>
#include <link/server.hpp>

namespace llarp
{
  struct AbstractRouter;

  namespace utp
  {
    LinkLayer_ptr
    NewServer(const SecretKey& routerEncSecret, GetRCFunc getrc,
              LinkMessageHandler h, SessionEstablishedHandler est,
              SessionRenegotiateHandler reneg, SignBufferFunc sign,
              TimeoutHandler timeout, SessionClosedHandler closed);

    LinkLayer_ptr
    NewServerFromRouter(AbstractRouter* r);
  }  // namespace utp
}  // namespace llarp

#endif
