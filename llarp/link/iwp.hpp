#ifndef LLARP_LINK_IWP_HPP
#define LLARP_LINK_IWP_HPP

#include <link/server.hpp>

#include <memory>

namespace llarp
{
  struct AbstractRouter;

  namespace iwp
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

  }  // namespace iwp
}  // namespace llarp

#endif
