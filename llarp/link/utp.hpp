#ifndef LLARP_LINK_UTP_HPP
#define LLARP_LINK_UTP_HPP

#include <memory>
#include <link/server.hpp>

namespace llarp
{
  struct AbstractRouter;

  namespace utp
  {
    std::unique_ptr< ILinkLayer >
    NewServer(llarp::Crypto* crypto, const SecretKey& routerEncSecret,
              llarp::GetRCFunc getrc, llarp::LinkMessageHandler h,
              llarp::SessionEstablishedHandler est,
              llarp::SessionRenegotiateHandler reneg,
              llarp::SignBufferFunc sign, llarp::TimeoutHandler timeout,
              llarp::SessionClosedHandler closed);

    std::unique_ptr< ILinkLayer >
    NewServerFromRouter(AbstractRouter* r);
  }  // namespace utp
}  // namespace llarp

#endif
