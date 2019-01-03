#ifndef LLARP_LINK_SERVER_HPP
#define LLARP_LINK_SERVER_HPP

#include <crypto.hpp>
#include <ev.h>
#include <link/session.hpp>
#include <logic.hpp>
#include <net.hpp>
#include <router_contact.hpp>
#include <threading.hpp>

#include <list>
#include <unordered_map>

namespace llarp
{
  /// handle a link layer message
  using LinkMessageHandler =
      std::function< bool(ILinkSession*, llarp_buffer_t) >;

  /// sign a buffer with identity key
  using SignBufferFunc = std::function< bool(Signature&, llarp_buffer_t) >;

  /// handle connection timeout
  using TimeoutHandler = std::function< void(ILinkSession*) >;

  /// get our RC
  using GetRCFunc = std::function< const llarp::RouterContact&(void) >;

  /// handler of session established
  using SessionEstablishedHandler = std::function< void(llarp::RouterContact) >;

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
    ForEachSession(std::function< void(const ILinkSession*) > visit) const;

    void
    ForEachSession(std::function< void(ILinkSession*) > visit);

    static void
    udp_tick(llarp_udp_io* udp)
    {
      static_cast< ILinkLayer* >(udp->user)->Pump();
    }

    static void
    udp_recv_from(llarp_udp_io* udp, const sockaddr* from, llarp_buffer_t buf)
    {
      if(!udp)
      {
        llarp::LogWarn("no udp set");
        return;
      }
      // maybe check from too?
      // no it's never null
      static_cast< ILinkLayer* >(udp->user)->RecvFrom(*from, buf.base, buf.sz);
    }

    void
    SendTo_LL(const llarp::Addr& to, llarp_buffer_t pkt)
    {
      llarp_ev_udp_sendto(&m_udp, to, pkt);
    }

    bool
    Configure(llarp_ev_loop* loop, const std::string& ifname, int af,
              uint16_t port);

    virtual ILinkSession*
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
    Start(llarp::Logic* l);

    void
    Stop();

    virtual const char*
    Name() const = 0;

    void
    CloseSessionTo(const RouterID& remote);

    void
    KeepAliveSessionTo(const RouterID& remote);

    bool
    SendTo(const RouterID& remote, llarp_buffer_t buf);

    bool
    GetOurAddressInfo(AddressInfo& addr) const;

    bool
    VisitSessionByPubkey(const RouterID& pk,
                         std::function< bool(ILinkSession*) > visit);

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
    EnsureKeys(const char* fpath);

    bool
    GenEphemeralKeys();

    void
    MapAddr(const RouterID& pk, ILinkSession* s);

    virtual void Tick(llarp_time_t)
    {
    }

    LinkMessageHandler HandleMessage;
    TimeoutHandler HandleTimeout;
    SignBufferFunc Sign;
    GetRCFunc GetOurRC;
    SessionEstablishedHandler SessionEstablished;
    SessionClosedHandler SessionClosed;
    SessionRenegotiateHandler SessionRenegotiate;

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
    using Lock  = util::NullLock;
    using Mutex = util::NullMutex;

    void
    PutSession(ILinkSession* s);

    llarp::Logic* m_Logic = nullptr;
    llarp_ev_loop* m_Loop = nullptr;
    Addr m_ourAddr;
    llarp_udp_io m_udp;
    SecretKey m_SecretKey;

    Mutex m_AuthedLinksMutex;
    std::unordered_multimap< RouterID, std::unique_ptr< ILinkSession >,
                             RouterID::Hash >
        m_AuthedLinks;
    Mutex m_PendingMutex;
    std::list< std::unique_ptr< ILinkSession > > m_Pending;
  };
}  // namespace llarp

#endif
