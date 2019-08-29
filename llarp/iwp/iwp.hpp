#ifndef LLARP_IWP_HPP
#define LLARP_IWP_HPP

#include <link/server.hpp>
#include <iwp/linklayer.hpp>
#include <memory>

namespace llarp
{
  namespace iwp
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

  }  // namespace iwp
}  // namespace llarp

#endif
