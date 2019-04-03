#ifndef LLARP_UTP_UTP_HPP
#define LLARP_UTP_UTP_HPP

#include <memory>
#include <link/server.hpp>

namespace llarp
{
  struct AbstractRouter;

  namespace utp
  {
    std::unique_ptr< ILinkLayer >
    NewServer(Crypto* crypto, const SecretKey& routerEncSecret, GetRCFunc getrc,
              LinkMessageHandler h, SessionEstablishedHandler est,
              SessionRenegotiateHandler reneg, SignBufferFunc sign,
              TimeoutHandler timeout, SessionClosedHandler closed);

    std::unique_ptr< ILinkLayer >
    NewServerFromRouter(AbstractRouter* r);
  }  // namespace utp
}  // namespace llarp

#endif
