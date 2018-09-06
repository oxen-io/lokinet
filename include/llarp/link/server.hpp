#ifndef LLARP_LINK_SERVER_HPP
#define LLARP_LINK_SERVER_HPP
#include <unordered_map>
#include <llarp/threading.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/crypto.hpp>
#include <llarp/net.hpp>
#include <llarp/ev.h>
#include <llarp/link/session.hpp>
#include <llarp/logic.h>

struct llarp_router;

namespace llarp
{
  struct ILinkLayer
  {
    virtual ~ILinkLayer();

    bool
    HasSessionTo(const PubKey& pk);

    bool
    HasSessionVia(const Addr& addr);

    static void
    udp_tick(llarp_udp_io* udp)
    {
      static_cast< ILinkLayer* >(udp->user)->Pump();
    }

    static void
    udp_recv_from(llarp_udp_io* udp, const sockaddr* from, const void* buf,
                  const ssize_t sz)
    {
      static_cast< ILinkLayer* >(udp->user)->RecvFrom(*from, buf, sz);
    }

    bool
    Configure(llarp_ev_loop* loop, const std::string& ifname, int af,
              uint16_t port);

    virtual std::unique_ptr< ILinkSession >
    NewOutboundSession(const RouterContact& rc, const AddressInfo& ai) = 0;

    virtual void
    Pump();

    virtual void
    RecvFrom(const Addr& from, const void* buf, size_t sz) = 0;

    bool
    PickAddress(const RouterContact& rc, AddressInfo& picked) const;

    void
    TryEstablishTo(const RouterContact& rc);

    bool
    Start(llarp_logic* l);

    void
    Stop();

    virtual const char*
    Name() const = 0;

    void
    CloseSessionTo(const PubKey& remote);

    void
    KeepAliveSessionTo(const PubKey& remote);

    bool
    SendTo(const PubKey& remote, llarp_buffer_t buf);

    bool
    GetOurAddressInfo(AddressInfo& addr) const;

    virtual uint16_t
    Rank() const = 0;

    virtual bool
    KeyGen(SecretKey&) = 0;

    const byte_t*
    TransportPubKey() const;

    const byte_t*
    TransportSecretKey() const;

    bool
    EnsureKeys(const char* fpath);

    void
    MapAddr(const Addr& addr, const PubKey& pk)
    {
      util::Lock l(m_LinksMutex);
      m_Links.insert(std::make_pair(pk, addr));
    }

   private:
    static void
    on_timer_tick(void* user, uint64_t orig, uint64_t left)
    {
      // timer cancelled
      if(left)
        return;
      static_cast< ILinkLayer* >(user)->Tick(orig, llarp_time_now_ms());
    }

    void
    Tick(uint64_t interval, llarp_time_t now);

    void
    ScheduleTick(uint64_t interval);

    uint32_t tick_id;

   protected:
    void
    PutSession(const Addr& addr, ILinkSession* s)
    {
      util::Lock l(m_SessionsMutex);
      m_Sessions.insert(
          std::make_pair(addr, std::unique_ptr< ILinkSession >(s)));
    }

    llarp_logic* m_Logic = nullptr;
    Addr m_ourAddr;
    llarp_udp_io m_udp;
    SecretKey m_SecretKey;
    util::Mutex m_LinksMutex;
    util::Mutex m_SessionsMutex;
    std::unordered_map< PubKey, Addr, PubKey::Hash > m_Links;
    std::unordered_map< Addr, std::unique_ptr< ILinkSession >, Addr::Hash >
        m_Sessions;
  };
}  // namespace llarp

#endif
