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
    NewInboundLink(const SecretKey& routerEncSecret, GetRCFunc getrc,
                   LinkMessageHandler h, SessionEstablishedHandler est,
                   SessionRenegotiateHandler reneg, SignBufferFunc sign,
                   TimeoutHandler timeout, SessionClosedHandler closed);
    LinkLayer_ptr
    NewOutboundLink(const SecretKey& routerEncSecret, GetRCFunc getrc,
                    LinkMessageHandler h, SessionEstablishedHandler est,
                    SessionRenegotiateHandler reneg, SignBufferFunc sign,
                    TimeoutHandler timeout, SessionClosedHandler closed);
    /// shim
    const auto NewServer = NewInboundLink;
  }  // namespace utp
}  // namespace llarp

#endif
