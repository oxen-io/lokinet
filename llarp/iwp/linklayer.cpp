#include <iwp/linklayer.hpp>
#include <iwp/session.hpp>

namespace llarp
{
  namespace iwp
  {
    LinkLayer::LinkLayer(const SecretKey& routerEncSecret, GetRCFunc getrc,
                         LinkMessageHandler h, SignBufferFunc sign,
                         SessionEstablishedHandler est,
                         SessionRenegotiateHandler reneg,
                         TimeoutHandler timeout, SessionClosedHandler closed,
                         bool allowInbound)
        : ILinkLayer(routerEncSecret, getrc, h, sign, est, reneg, timeout,
                     closed)
        , permitInbound{allowInbound}
    {
    }

    LinkLayer::~LinkLayer() = default;

    void
    LinkLayer::Pump()
    {
      ILinkLayer::Pump();
    }

    const char*
    LinkLayer::Name() const
    {
      return "iwp";
    }

    bool
    LinkLayer::KeyGen(SecretKey& k)
    {
      k.Zero();
      CryptoManager::instance()->encryption_keygen(k);
      return !k.IsZero();
    }

    uint16_t
    LinkLayer::Rank() const
    {
      return 2;
    }

    bool
    LinkLayer::Start(std::shared_ptr< Logic > l)
    {
      return ILinkLayer::Start(l);
    }

    void
    LinkLayer::RecvFrom(const Addr& from, const void* pkt, size_t sz)
    {
      std::shared_ptr< ILinkSession > session;
      {
        util::Lock lock(&m_PendingMutex);
        if(m_Pending.count(from) == 0)
        {
          m_Pending.insert({from, std::make_shared< Session >(this, from)});
        }
        session = m_Pending.find(from)->second;
      }
      const llarp_buffer_t buf{pkt, sz};
      session->Recv_LL(buf);
    }

    std::shared_ptr< ILinkSession >
    LinkLayer::NewOutboundSession(const RouterContact& rc,
                                  const AddressInfo& ai)
    {
      return std::make_shared< Session >(this, rc, ai);
    }
  }  // namespace iwp
}  // namespace llarp
