#pragma once

#include <llarp/crypto/types.hpp>
#include <llarp/ev/ev.hpp>
#include "session.hpp"
#include <llarp/net/sock_addr.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/util/status.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/config/key_manager.hpp>

#include <list>
#include <memory>
#include <unordered_map>

namespace llarp
{
  /// handle a link layer message. this allows for the message to be handled by "upper layers"
  ///
  /// currently called from iwp::Session when messages are sent or received.
  using LinkMessageHandler = std::function<bool(ILinkSession*, const llarp_buffer_t&)>;

  /// sign a buffer with identity key. this function should take the given `llarp_buffer_t` and
  /// sign it, prividing the signature in the out variable `Signature&`.
  ///
  /// currently called from iwp::Session for signing LIMs (link introduction messages)
  using SignBufferFunc = std::function<bool(Signature&, const llarp_buffer_t&)>;

  /// handle connection timeout
  ///
  /// currently called from ILinkLayer::Pump() when an unestablished session times out
  using TimeoutHandler = std::function<void(ILinkSession*)>;

  /// get our RC
  ///
  /// currently called by iwp::Session to include as part of a LIM (link introduction message)
  using GetRCFunc = std::function<const llarp::RouterContact&(void)>;

  /// handler of session established
  /// return false to reject
  /// return true to accept
  ///
  /// currently called in iwp::Session when a valid LIM is received.
  using SessionEstablishedHandler = std::function<bool(ILinkSession*, bool)>;

  /// f(new, old)
  /// handler of session renegotiation
  /// returns true if the new rc is valid
  /// returns false otherwise and the session is terminated
  ///
  /// currently called from iwp::Session when we receive a renegotiation LIM
  using SessionRenegotiateHandler = std::function<bool(llarp::RouterContact, llarp::RouterContact)>;

  /// handles close of all sessions with pubkey
  ///
  /// Note that this handler is called while m_AuthedLinksMutex is held
  ///
  /// currently called from iwp::ILinkSession when a previously established session times out
  using SessionClosedHandler = std::function<void(llarp::RouterID)>;

  /// notifies router that a link session has ended its pump and we should flush
  /// messages to upper layers
  ///
  /// currently called at the end of every iwp::Session::Pump() call
  using PumpDoneHandler = std::function<void(void)>;

  using Work_t = std::function<void(void)>;
  /// queue work to worker thread
  using WorkerFunc_t = std::function<void(Work_t)>;

  /// before connection hook, called before we try connecting via outbound link
  using BeforeConnectFunc_t = std::function<void(llarp::RouterContact)>;

  struct ILinkLayer
  {
    ILinkLayer(
        std::shared_ptr<KeyManager> keyManager,
        GetRCFunc getrc,
        LinkMessageHandler handler,
        SignBufferFunc signFunc,
        BeforeConnectFunc_t before,
        SessionEstablishedHandler sessionEstablish,
        SessionRenegotiateHandler renegotiate,
        TimeoutHandler timeout,
        SessionClosedHandler closed,
        PumpDoneHandler pumpDone,
        WorkerFunc_t doWork);
    virtual ~ILinkLayer();

    /// get current time via event loop
    llarp_time_t
    Now() const
    {
      return m_Loop->time_now();
    }

    bool
    HasSessionTo(const RouterID& pk);

    void
    ForEachSession(std::function<void(const ILinkSession*)> visit, bool randomize = false) const
        EXCLUDES(m_AuthedLinksMutex);

    void
    ForEachSession(std::function<void(ILinkSession*)> visit) EXCLUDES(m_AuthedLinksMutex);

    void
    SendTo_LL(const SockAddr& to, const llarp_buffer_t& pkt);

    virtual bool
    Configure(EventLoop_ptr loop, const std::string& ifname, int af, uint16_t port);

    virtual std::shared_ptr<ILinkSession>
    NewOutboundSession(const RouterContact& rc, const AddressInfo& ai) = 0;

    /// fetch a session by the identity pubkey it claims
    std::shared_ptr<ILinkSession>
    FindSessionByPubkey(RouterID pk);

    virtual void
    Pump();

    virtual void
    RecvFrom(const SockAddr& from, ILinkSession::Packet_t pkt) = 0;

    bool
    PickAddress(const RouterContact& rc, AddressInfo& picked) const;

    bool
    TryEstablishTo(RouterContact rc);

    bool
    Start();

    virtual void
    Stop();

    virtual const char*
    Name() const = 0;

    util::StatusObject
    ExtractStatus() const EXCLUDES(m_AuthedLinksMutex);

    void
    CloseSessionTo(const RouterID& remote);

    void
    KeepAliveSessionTo(const RouterID& remote);

    virtual bool
    SendTo(
        const RouterID& remote,
        const llarp_buffer_t& buf,
        ILinkSession::CompletionHandler completed);

    virtual bool
    GetOurAddressInfo(AddressInfo& addr) const;

    bool
    VisitSessionByPubkey(const RouterID& pk, std::function<bool(ILinkSession*)> visit)
        EXCLUDES(m_AuthedLinksMutex);

    virtual uint16_t
    Rank() const = 0;

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
      for (const auto& ai : other.addrs)
        if (ai.dialect == us)
          return true;
      return false;
    }

    virtual bool
    MapAddr(const RouterID& pk, ILinkSession* s);

    void
    Tick(llarp_time_t now);

    LinkMessageHandler HandleMessage;
    TimeoutHandler HandleTimeout;
    SignBufferFunc Sign;
    GetRCFunc GetOurRC;
    BeforeConnectFunc_t BeforeConnect;
    SessionEstablishedHandler SessionEstablished;
    SessionClosedHandler SessionClosed;
    SessionRenegotiateHandler SessionRenegotiate;
    PumpDoneHandler PumpDone;
    std::shared_ptr<KeyManager> keyManager;
    WorkerFunc_t QueueWork;

    bool
    operator<(const ILinkLayer& other) const
    {
      return Rank() < other.Rank() || Name() < other.Name() || m_ourAddr < other.m_ourAddr;
    }

    /// called by link session to remove a pending session who is timed out
    // void
    // RemovePending(ILinkSession* s) EXCLUDES(m_PendingMutex);

    /// count the number of sessions that are yet to be fully connected
    size_t
    NumberOfPendingSessions() const
    {
      Lock_t lock(m_PendingMutex);
      return m_Pending.size();
    }

    // Returns the file description of the UDP server, if available.
    std::optional<int>
    GetUDPFD() const;

   private:
    const SecretKey& m_RouterEncSecret;

   protected:
#ifdef TRACY_ENABLE
    using Lock_t = std::lock_guard<LockableBase(std::mutex)>;
    using Mutex_t = std::mutex;
#else
    using Lock_t = util::NullLock;
    using Mutex_t = util::NullMutex;
#endif
    bool
    PutSession(const std::shared_ptr<ILinkSession>& s);

    EventLoop_ptr m_Loop;
    SockAddr m_ourAddr;
    std::shared_ptr<llarp::UDPHandle> m_udp;
    SecretKey m_SecretKey;

    using AuthedLinks = std::unordered_multimap<RouterID, std::shared_ptr<ILinkSession>>;
    using Pending = std::unordered_map<SockAddr, std::shared_ptr<ILinkSession>>;
    mutable DECLARE_LOCK(Mutex_t, m_AuthedLinksMutex, ACQUIRED_BEFORE(m_PendingMutex));
    AuthedLinks m_AuthedLinks GUARDED_BY(m_AuthedLinksMutex);
    mutable DECLARE_LOCK(Mutex_t, m_PendingMutex, ACQUIRED_AFTER(m_AuthedLinksMutex));
    Pending m_Pending GUARDED_BY(m_PendingMutex);

    std::unordered_map<SockAddr, llarp_time_t> m_RecentlyClosed;

   private:
    std::shared_ptr<int> m_repeater_keepalive;
  };

  using LinkLayer_ptr = std::shared_ptr<ILinkLayer>;
}  // namespace llarp
