#include <utp/utp.hpp>

#include <router/abstractrouter.hpp>
#include <util/meta/memfn.hpp>
#include <utp/linklayer.hpp>

namespace llarp
{
  namespace utp
  {
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

  }  // namespace utp

}  // namespace llarp
