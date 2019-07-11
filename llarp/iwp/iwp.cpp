#include <iwp/iwp.hpp>
#include <iwp/linklayer.hpp>
#include <router/abstractrouter.hpp>
#include <util/memfn.hpp>

namespace llarp
{
  namespace iwp
  {
    std::unique_ptr< ILinkLayer >
    NewServerFromRouter(AbstractRouter* r)
    {
      return NewServer(
          r->encryption(), std::bind(&AbstractRouter::rc, r),
          util::memFn(&AbstractRouter::HandleRecvLinkMessageBuffer, r),
          util::memFn(&AbstractRouter::OnSessionEstablished, r),
          util::memFn(&AbstractRouter::CheckRenegotiateValid, r),
          util::memFn(&AbstractRouter::Sign, r),
          util::memFn(&AbstractRouter::OnConnectTimeout, r),
          util::memFn(&AbstractRouter::SessionClosed, r));
    }

    std::unique_ptr< ILinkLayer >
    NewServer(const SecretKey& enckey, GetRCFunc getrc, LinkMessageHandler h,
              SessionEstablishedHandler est, SessionRenegotiateHandler reneg,
              SignBufferFunc sign, TimeoutHandler t,
              SessionClosedHandler closed)
    {
      (void)enckey;
      (void)getrc;
      (void)h;
      (void)est;
      (void)reneg;
      (void)sign;
      (void)t;
      (void)closed;
      // TODO: implement me
      return nullptr;
    }
  }  // namespace iwp
}  // namespace llarp
