#include <iwp/iwp.hpp>
#include <iwp/linklayer.hpp>
#include <router/abstractrouter.hpp>
#include <util/meta/memfn.hpp>

namespace llarp
{
  namespace iwp
  {
    LinkLayer_ptr
    NewInboundLink(const SecretKey& routerEncSecret, GetRCFunc getrc,
                   LinkMessageHandler h, SignBufferFunc sign,
                   SessionEstablishedHandler est,
                   SessionRenegotiateHandler reneg, TimeoutHandler timeout,
                   SessionClosedHandler closed)
    {
      return std::make_shared< LinkLayer >(routerEncSecret, getrc, h, sign, est,
                                           reneg, timeout, closed, true);
    }

    LinkLayer_ptr
    NewOutboundLink(const SecretKey& routerEncSecret, GetRCFunc getrc,
                    LinkMessageHandler h, SignBufferFunc sign,
                    SessionEstablishedHandler est,
                    SessionRenegotiateHandler reneg, TimeoutHandler timeout,
                    SessionClosedHandler closed)
    {
      return std::make_shared< LinkLayer >(routerEncSecret, getrc, h, sign, est,
                                           reneg, timeout, closed, false);
    }
  }  // namespace iwp
}  // namespace llarp
