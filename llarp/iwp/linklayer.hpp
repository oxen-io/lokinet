#ifndef LLARP_IWP_LINKLAYER_HPP
#define LLARP_IWP_LINKLAYER_HPP

#include <constants/link_layer.hpp>
#include <crypto/crypto.hpp>
#include <crypto/encrypted.hpp>
#include <crypto/types.hpp>
#include <link/server.hpp>
#include <config/key_manager.hpp>
#include <util/thread/queue.hpp>
#include <memory>

#include <ev/ev.hpp>

namespace llarp::iwp
{
  struct Session;

  struct LinkLayer final : public ILinkLayer
  {
    LinkLayer(
        std::shared_ptr<KeyManager> keyManager,
        std::shared_ptr<EventLoop> ev,
        GetRCFunc getrc,
        LinkMessageHandler h,
        SignBufferFunc sign,
        BeforeConnectFunc_t before,
        SessionEstablishedHandler est,
        SessionRenegotiateHandler reneg,
        TimeoutHandler timeout,
        SessionClosedHandler closed,
        PumpDoneHandler pumpDone,
        WorkerFunc_t dowork,
        bool permitInbound);

    ~LinkLayer() override;

    std::shared_ptr<ILinkSession>
    NewOutboundSession(const RouterContact& rc, const AddressInfo& ai) override;

    const char*
    Name() const override;

    uint16_t
    Rank() const override;

    void
    RecvFrom(const SockAddr& from, ILinkSession::Packet_t pkt) override;

    bool
    MapAddr(const RouterID& pk, ILinkSession* s) override;

    void
    UnmapAddr(const IpAddress& addr);

    void
    WakeupPlaintext();

    void
    AddWakeup(std::weak_ptr<Session> peer);

   private:
    void
    HandleWakeupPlaintext();

    EventLoopWakeup* const m_Wakeup;
    llarp::thread::Queue<std::weak_ptr<Session>> m_PlaintextRecv;
    std::unordered_map<IpAddress, RouterID, IpAddress::Hash> m_AuthedAddrs;
    const bool permitInbound;
  };

  using LinkLayer_ptr = std::shared_ptr<LinkLayer>;
}  // namespace llarp::iwp

#endif
