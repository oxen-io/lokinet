#include "iwp.hpp"
#include "linklayer.hpp"
#include <memory>
#include <llarp/router/abstractrouter.hpp>

namespace llarp
{
  namespace iwp
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
        WorkerFunc_t work)
    {
      return std::make_shared<LinkLayer>(
          keyManager,
          loop,
          getrc,
          h,
          sign,
          before,
          est,
          reneg,
          timeout,
          closed,
          pumpDone,
          work,
          true);
    }

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
        WorkerFunc_t work)
    {
      return std::make_shared<LinkLayer>(
          keyManager,
          loop,
          getrc,
          h,
          sign,
          before,
          est,
          reneg,
          timeout,
          closed,
          pumpDone,
          work,
          false);
    }
  }  // namespace iwp
}  // namespace llarp
