#ifndef LLARP_LINK_SERVER_HPP
#define LLARP_LINK_SERVER_HPP

#include <crypto/types.hpp>
#include <ev/ev.h>
#include <link/session.hpp>
#include <net/net.hpp>
#include <router_contact.hpp>
#include <util/status.hpp>
#include <util/thread/logic.hpp>
#include <util/thread/threading.hpp>

#include <list>
#include <memory>
#include <unordered_map>

namespace llarp
{
  /// handle a link layer message
  using LinkMessageHandler =
      std::function< bool(ILinkSession*, const llarp_buffer_t&) >;

  /// sign a buffer with identity key
  using SignBufferFunc =
      std::function< bool(Signature&, const llarp_buffer_t&) >;

  /// handle connection timeout
  using TimeoutHandler = std::function< void(ILinkSession*) >;

  /// get our RC
  using GetRCFunc = std::function< const llarp::RouterContact&(void) >;

  /// handler of session established
  /// return false to reject
  /// return true to accept
  using SessionEstablishedHandler = std::function< bool(ILinkSession*) >;

  /// f(new, old)
  /// handler of session renegotiation
  /// returns true if the new rc is valid
  /// returns false otherwise and the session is terminated
  using SessionRenegotiateHandler =
      std::function< bool(llarp::RouterContact, llarp::RouterContact) >;

  /// handles close of all sessions with pubkey
  using SessionClosedHandler = std::function< void(llarp::RouterID) >;

  struct ILinkLayer
  {
    ILinkLayer(const SecretKey& routerEncSecret, GetRCFunc getrc,
               LinkMessageHandler handler, SignBufferFunc signFunc,
               SessionEstablishedHandler sessionEstablish,
               SessionRenegotiateHandler renegotiate, TimeoutHandler timeout,
               SessionClosedHandler closed);
    virtual ~ILinkLayer();

    /// get current time via event loop
    llarp_time_t
    Now() const
    {
      return llarp_ev_loop_time_now_ms(m_Loop);
    }

    bool
    HasSessionTo(const RouterID& pk);

    bool
    HasSessionVia(const Addr& addr);

    void
    ForEachSession(std::function< void(const ILinkSession*) > visit,
                   bool randomize = false) const
        LOCKS_EXCLUDED(m_AuthedLinksMutex);

    void
    ForEachSession(std::function< void(ILinkSession*) > visit)
        LOCKS_EXCLUDED(m_AuthedLinksMutex);

    static void
    udp_tick(llarp_udp_io* udp)
    {
      static_cast< ILinkLayer* >(udp->user)->Pump();
    }

    static void
    udp_recv_from(llarp_udp_io* udp, const sockaddr* from, ManagedBuffer buf)
    {
      if(!udp)
      {
        llarp::LogWarn("no udp set");
        return;
      }
      const llarp::Addr srcaddr(*from);
      // maybe check from too?
      // no it's never null
      static_cast< ILinkLayer* >(udp->user)->RecvFrom(
          srcaddr, buf.underlying.base, buf.underlying.sz);
    }

    void
    SendTo_LL(const llarp::Addr& to, const llarp_buffer_t& pkt)
    {
      llarp_ev_udp_sendto(&m_udp, to, pkt);
    }

    virtual bool
    Configure(llarp_ev_loop_ptr loop, const std::string& ifname, int af,
              uint16_t port);

    virtual std::shared_ptr< ILinkSession >
    NewOutboundSession(const RouterContact& rc, const AddressInfo& ai) = 0;

    virtual void
    Pump();

    virtual void
    RecvFrom(const Addr& from, const void* buf, size_t sz) = 0;

    bool
    PickAddress(const RouterContact& rc, AddressInfo& picked) const;

    bool
    TryEstablishTo(RouterContact rc);

    virtual bool
    Start(std::shared_ptr< llarp::Logic > l);

    virtual void
    Stop();

    virtual const char*
    Name() const = 0;

    util::StatusObject
    ExtractStatus() const LOCKS_EXCLUDED(m_AuthedLinksMutex);

    void
    CloseSessionTo(const RouterID& remote);

    void
    KeepAliveSessionTo(const RouterID& remote);

    virtual bool
    SendTo(const RouterID& remote, const llarp_buffer_t& buf,
           ILinkSession::CompletionHandler completed);

    virtual bool
    GetOurAddressInfo(AddressInfo& addr) const;

    bool
    VisitSessionByPubkey(const RouterID& pk,
                         std::function< bool(ILinkSession*) > visit)
        LOCKS_EXCLUDED(m_AuthedLinksMutex);

    virtual uint16_t
    Rank() const = 0;

    virtual bool
    KeyGen(SecretKey&) = 0;

    const byte_t*
    TransportPubKey() const;

    const SecretKey&
    RouterEncryptionSecret() const
    {
      return m_RouterEncSecret;
    }

    const SecretKey&
    TransportSecretKey() const;

    bool
    IsCompatable(const llarp::RouterContact& other) const
    {
      const std::string us = Name();
      for(const auto& ai : other.addrs)
        if(ai.dialect == us)
          return true;
      return false;
    }

    bool
    EnsureKeys(const char* fpath);

    bool
    GenEphemeralKeys();

    virtual bool
    MapAddr(const RouterID& pk, ILinkSession* s);

    void
    Tick(llarp_time_t now);

    LinkMessageHandler HandleMessage;
    TimeoutHandler HandleTimeout;
    SignBufferFunc Sign;
    GetRCFunc GetOurRC;
    SessionEstablishedHandler SessionEstablished;
    SessionClosedHandler SessionClosed;
    SessionRenegotiateHandler SessionRenegotiate;

    std::shared_ptr< Logic >
    logic()
    {
      return m_Logic;
    }

    bool
    operator<(const ILinkLayer& other) const
    {
      return Rank() < other.Rank() || Name() < other.Name()
          || m_ourAddr < other.m_ourAddr;
    }

    /// called by link session to remove a pending session who is timed out
    // void
    // RemovePending(ILinkSession* s) LOCKS_EXCLUDED(m_PendingMutex);

   private:
    static void
    on_timer_tick(void* user, uint64_t orig, uint64_t left)
    {
      // timer cancelled
      if(left)
        return;
      static_cast< ILinkLayer* >(user)->OnTick(orig);
    }

    void
    OnTick(uint64_t interval);

    void
    ScheduleTick(uint64_t interval);

    uint32_t tick_id;
    const SecretKey& m_RouterEncSecret;

   protected:
    using Lock  = util::Lock;
    using Mutex = util::Mutex;

    bool
    PutSession(const std::shared_ptr< ILinkSession >& s);

    std::shared_ptr< llarp::Logic > m_Logic = nullptr;
    llarp_ev_loop_ptr m_Loop;
    Addr m_ourAddr;
    llarp_udp_io m_udp;
    SecretKey m_SecretKey;

    using AuthedLinks =
        std::unordered_multimap< RouterID, std::shared_ptr< ILinkSession >,
                                 RouterID::Hash >;
    using Pending =
        std::unordered_multimap< llarp::Addr, std::shared_ptr< ILinkSession >,
                                 llarp::Addr::Hash >;

    mutable Mutex m_AuthedLinksMutex ACQUIRED_BEFORE(m_PendingMutex);
    AuthedLinks m_AuthedLinks GUARDED_BY(m_AuthedLinksMutex);
    mutable Mutex m_PendingMutex ACQUIRED_AFTER(m_AuthedLinksMutex);
    Pending m_Pending GUARDED_BY(m_PendingMutex);
  };

  using LinkLayer_ptr = std::shared_ptr< ILinkLayer >;
}  // namespace llarp

#endif
