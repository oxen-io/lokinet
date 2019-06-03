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

    LinkLayer_ptr
    NewServerFromRouter(AbstractRouter* r)
    {
      return NewServer(
          r->encryption(), util::memFn(&AbstractRouter::rc, r),
          util::memFn(&AbstractRouter::HandleRecvLinkMessageBuffer, r),
          util::memFn(&AbstractRouter::OnSessionEstablished, r),
          util::memFn(&AbstractRouter::CheckRenegotiateValid, r),
          util::memFn(&AbstractRouter::Sign, r),
          util::memFn(&AbstractRouter::OnConnectTimeout, r),
          util::memFn(&AbstractRouter::SessionClosed, r));
    }

  }  // namespace utp

}  // namespace llarp
