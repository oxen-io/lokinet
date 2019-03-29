#include <link/iwp.hpp>
#include <link/iwp_internal.hpp>
#include <router/abstractrouter.hpp>

namespace llarp
{
  namespace iwp
  {
    std::unique_ptr< ILinkLayer >
    NewServerFromRouter(AbstractRouter* r)
    {
      using namespace std::placeholders;
      return NewServer(
          r->crypto(), r->encryption(), std::bind(&AbstractRouter::rc, r),
          std::bind(&AbstractRouter::HandleRecvLinkMessageBuffer, r, _1, _2),
          std::bind(&AbstractRouter::OnSessionEstablished, r, _1),
          std::bind(&AbstractRouter::CheckRenegotiateValid, r, _1, _2),
          std::bind(&AbstractRouter::Sign, r, _1, _2),
          std::bind(&AbstractRouter::OnConnectTimeout, r, _1),
          std::bind(&AbstractRouter::SessionClosed, r, _1));
    }

    std::unique_ptr< ILinkLayer >
    NewServer(Crypto* c, const SecretKey& enckey, GetRCFunc getrc,
              LinkMessageHandler h, SessionEstablishedHandler est,
              SessionRenegotiateHandler reneg, SignBufferFunc sign,
              TimeoutHandler t, SessionClosedHandler closed)
    {
      (void)c;
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
