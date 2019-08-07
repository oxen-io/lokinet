#ifndef LLARP_MEMPIPE_MEMPIPE_HPP
#define LLARP_MEMPIPE_MEMPIPE_HPP
#include <memory>
#include <link/server.hpp>

namespace llarp
{
  namespace mempipe
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
  }  // namespace mempipe
}  // namespace llarp

#endif