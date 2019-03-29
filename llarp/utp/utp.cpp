#include <utp/utp.hpp>

#include <utp/linklayer.hpp>
#include <router/abstractrouter.hpp>

namespace llarp
{
  namespace utp
  {
    using namespace std::placeholders;

    std::unique_ptr< ILinkLayer >
    NewServer(Crypto* crypto, const SecretKey& routerEncSecret, GetRCFunc getrc,
              LinkMessageHandler h, SessionEstablishedHandler est,
              SessionRenegotiateHandler reneg, SignBufferFunc sign,
              TimeoutHandler timeout, SessionClosedHandler closed)
    {
      return std::unique_ptr< ILinkLayer >(
          new LinkLayer(crypto, routerEncSecret, getrc, h, sign, est, reneg,
                        timeout, closed));
    }

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

  }  // namespace utp

}  // namespace llarp
