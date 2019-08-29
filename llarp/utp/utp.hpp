#ifndef LLARP_UTP_UTP_HPP
#define LLARP_UTP_UTP_HPP

#include <memory>
#include <link/server.hpp>

namespace llarp
{
  namespace utp
  {
    LinkLayer_ptr
    NewInboundLink(const SecretKey& routerEncSecret, GetRCFunc getrc,
                   LinkMessageHandler h, SignBufferFunc sign,
                   SessionEstablishedHandler est,
                   SessionRenegotiateHandler reneg, TimeoutHandler timeout,
                   SessionClosedHandler closed);
    LinkLayer_ptr
    NewOutboundLink(const SecretKey& routerEncSecret, GetRCFunc getrc,
                    LinkMessageHandler h, SignBufferFunc sign,
                    SessionEstablishedHandler est,
                    SessionRenegotiateHandler reneg, TimeoutHandler timeout,
                    SessionClosedHandler closed);
    /// shim
    const auto NewServer = NewInboundLink;
  }  // namespace utp
}  // namespace llarp

#endif
