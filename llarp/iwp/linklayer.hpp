#pragma once

#include <llarp/constants/link_layer.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/crypto/encrypted.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/link/server.hpp>
#include <llarp/config/key_manager.hpp>

#include <memory>

#include <llarp/ev/ev.hpp>

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
    UnmapAddr(const SockAddr& addr);

    void
    WakeupPlaintext();

    void
    AddWakeup(std::weak_ptr<Session> peer);

    std::string
    PrintableName() const;

   private:
    void
    HandleWakeupPlaintext();

    const std::shared_ptr<EventLoopWakeup> m_Wakeup;
    std::unordered_map<SockAddr, std::weak_ptr<Session>> m_PlaintextRecv;
    std::unordered_map<SockAddr, RouterID> m_AuthedAddrs;
    const bool m_Inbound;
  };

  using LinkLayer_ptr = std::shared_ptr<LinkLayer>;
}  // namespace llarp::iwp
