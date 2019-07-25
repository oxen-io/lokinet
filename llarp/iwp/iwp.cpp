#include <iwp/iwp.hpp>
#include <iwp/linklayer.hpp>
#include <router/abstractrouter.hpp>
#include <util/memfn.hpp>

namespace llarp
{
  namespace iwp
  {
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
