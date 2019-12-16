#include <iwp/iwp.hpp>
#include <iwp/linklayer.hpp>
#include <memory>
#include <router/abstractrouter.hpp>
#include <util/meta/memfn.hpp>

namespace llarp
{
  namespace iwp
  {
    LinkLayer_ptr
    NewInboundLink(std::shared_ptr< KeyManager > keyManager, GetRCFunc getrc,
                   LinkMessageHandler h, SignBufferFunc sign,
                   SessionEstablishedHandler est,
                   SessionRenegotiateHandler reneg, TimeoutHandler timeout,
                   SessionClosedHandler closed, PumpDoneHandler pumpDone)
    {
      return std::make_shared< LinkLayer >(keyManager, getrc, h, sign, est,
                                           reneg, timeout, closed, pumpDone,
                                           true);
    }

    LinkLayer_ptr
    NewOutboundLink(std::shared_ptr< KeyManager > keyManager, GetRCFunc getrc,
                    LinkMessageHandler h, SignBufferFunc sign,
                    SessionEstablishedHandler est,
                    SessionRenegotiateHandler reneg, TimeoutHandler timeout,
                    SessionClosedHandler closed, PumpDoneHandler pumpDone)
    {
      return std::make_shared< LinkLayer >(keyManager, getrc, h, sign, est,
                                           reneg, timeout, closed, pumpDone,
                                           false);
    }
  }  // namespace iwp
}  // namespace llarp
