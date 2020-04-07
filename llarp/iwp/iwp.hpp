#ifndef LLARP_IWP_HPP
#define LLARP_IWP_HPP

#include <link/server.hpp>
#include <iwp/linklayer.hpp>
#include <memory>
#include <config/key_manager.hpp>

namespace llarp
{
  namespace iwp
  {
    LinkLayer_ptr
    NewInboundLink(
        std::shared_ptr<KeyManager> keyManager,
        GetRCFunc getrc,
        LinkMessageHandler h,
        SignBufferFunc sign,
        SessionEstablishedHandler est,
        SessionRenegotiateHandler reneg,
        TimeoutHandler timeout,
        SessionClosedHandler closed,
        PumpDoneHandler pumpDone);
    LinkLayer_ptr
    NewOutboundLink(
        std::shared_ptr<KeyManager> keyManager,
        GetRCFunc getrc,
        LinkMessageHandler h,
        SignBufferFunc sign,
        SessionEstablishedHandler est,
        SessionRenegotiateHandler reneg,
        TimeoutHandler timeout,
        SessionClosedHandler closed,
        PumpDoneHandler pumpDone);

  }  // namespace iwp
}  // namespace llarp

#endif
