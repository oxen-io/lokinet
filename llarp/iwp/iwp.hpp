#ifndef LLARP_IWP_HPP
#define LLARP_IWP_HPP

#include <link/server.hpp>
#include <iwp/linklayer.hpp>
#include <memory>
#include <config/key_manager.hpp>

namespace llarp::iwp
{
  LinkLayer_ptr
  NewInboundLink(
      std::shared_ptr<KeyManager> keyManager,
      std::shared_ptr<EventLoop> loop,
      GetRCFunc getrc,
      LinkMessageHandler h,
      SignBufferFunc sign,
      BeforeConnectFunc_t before,
      SessionEstablishedHandler est,
      SessionRenegotiateHandler reneg,
      TimeoutHandler timeout,
      SessionClosedHandler closed,
      PumpDoneHandler pumpDone,
      WorkerFunc_t work);

  LinkLayer_ptr
  NewOutboundLink(
      std::shared_ptr<KeyManager> keyManager,
      std::shared_ptr<EventLoop> loop,
      GetRCFunc getrc,
      LinkMessageHandler h,
      SignBufferFunc sign,
      BeforeConnectFunc_t before,
      SessionEstablishedHandler est,
      SessionRenegotiateHandler reneg,
      TimeoutHandler timeout,
      SessionClosedHandler closed,
      PumpDoneHandler pumpDone,
      WorkerFunc_t work);

}  // namespace llarp::iwp

#endif
