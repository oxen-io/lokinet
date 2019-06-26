#include <utp/utp.hpp>

#include <router/abstractrouter.hpp>
#include <util/memfn.hpp>
#include <utp/linklayer.hpp>

namespace llarp
{
  namespace utp
  {
    LinkLayer_ptr
    NewServer(const SecretKey& routerEncSecret, GetRCFunc getrc,
              LinkMessageHandler h, SessionEstablishedHandler est,
              SessionRenegotiateHandler reneg, SignBufferFunc sign,
              TimeoutHandler timeout, SessionClosedHandler closed)
    {
      return std::make_shared< LinkLayer >(routerEncSecret, getrc, h, sign, est,
                                           reneg, timeout, closed);
    }

  }  // namespace utp

}  // namespace llarp
