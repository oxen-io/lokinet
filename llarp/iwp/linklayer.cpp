#include <iwp/linklayer.hpp>
#include <iwp/session.hpp>
#include <unordered_set>

namespace llarp
{
  namespace iwp
  {
    LinkLayer::LinkLayer(const SecretKey& routerEncSecret, GetRCFunc getrc,
                         LinkMessageHandler h, SignBufferFunc sign,
                         SessionEstablishedHandler est,
                         SessionRenegotiateHandler reneg,
                         TimeoutHandler timeout, SessionClosedHandler closed,
                         PumpDoneHandler pumpDone, bool allowInbound)
        : ILinkLayer(routerEncSecret, getrc, h, sign, est, reneg, timeout,
                     closed, pumpDone)
        , permitInbound{allowInbound}
    {
    }

    LinkLayer::~LinkLayer() = default;

    void
    LinkLayer::Pump()
    {
      std::unordered_set< RouterID, RouterID::Hash > sessions;
      {
        ACQUIRE_LOCK(Lock_t l, m_AuthedLinksMutex);
        auto itr = m_AuthedLinks.begin();
        while(itr != m_AuthedLinks.end())
        {
          const RouterID r{itr->first};
          sessions.emplace(r);
          ++itr;
        }
      }
      ILinkLayer::Pump();
      {
        ACQUIRE_LOCK(Lock_t l, m_AuthedLinksMutex);
        for(const auto& pk : sessions)
        {
          if(m_AuthedLinks.count(pk) == 0)
          {
            // all sessions were removed
            SessionClosed(pk);
          }
        }
      }
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

    void
    LinkLayer::QueueWork(std::function< void(void) > func)
    {
      m_Worker->addJob(func);
    }

    void
    LinkLayer::RecvFrom(const Addr& from, byte_t* ptr, size_t sz)
    {
      std::shared_ptr< ILinkSession > session;
      auto itr = m_AuthedAddrs.find(from);
      if(itr == m_AuthedAddrs.end())
      {
        ACQUIRE_LOCK(Lock_t lock, m_PendingMutex);
        if(m_Pending.count(from) == 0)
        {
          if(not permitInbound)
            return;
          m_Pending.insert({from, std::make_shared< Session >(this, from)});
        }
        session = m_Pending.find(from)->second;
      }
      else
      {
        ACQUIRE_LOCK(Lock_t lock, m_AuthedLinksMutex);
        auto range = m_AuthedLinks.equal_range(itr->second);
        session    = range.first->second;
      }
      if(session)
      {
        session->Recv_LL(ptr, sz);
      }
    }

    bool
    LinkLayer::MapAddr(const RouterID& r, ILinkSession* s)
    {
      if(!ILinkLayer::MapAddr(r, s))
        return false;
      m_AuthedAddrs.emplace(s->GetRemoteEndpoint(), r);
      return true;
    }

    void
    LinkLayer::UnmapAddr(const Addr& a)
    {
      m_AuthedAddrs.erase(a);
    }

    std::shared_ptr< ILinkSession >
    LinkLayer::NewOutboundSession(const RouterContact& rc,
                                  const AddressInfo& ai)
    {
      return std::make_shared< Session >(this, rc, ai);
    }
  }  // namespace iwp
}  // namespace llarp
