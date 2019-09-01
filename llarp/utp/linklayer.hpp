#ifndef LLARP_UTP_LINKLAYER_HPP
#define LLARP_UTP_LINKLAYER_HPP

#include <utp/inbound_message.hpp>

#include <crypto/crypto.hpp>
#include <crypto/types.hpp>
#include <link/server.hpp>
#include <utp.h>

#include <deque>

namespace llarp
{
  namespace utp
  {
    struct LinkLayer final : public ILinkLayer
    {
      utp_context* _utp_ctx = nullptr;

      // low level read callback
      static uint64
      OnRead(utp_callback_arguments* arg);

      // low level sendto callback
      static uint64
      SendTo(utp_callback_arguments* arg);

      /// error callback
      static uint64
      OnError(utp_callback_arguments* arg);

      /// state change callback
      static uint64
      OnStateChange(utp_callback_arguments*);

      static uint64
      OnConnect(utp_callback_arguments*);

      /// accept callback
      static uint64
      OnAccept(utp_callback_arguments*);

      /// logger callback
      static uint64
      OnLog(utp_callback_arguments* arg);

      /// construct
      LinkLayer(const SecretKey& routerEncSecret, GetRCFunc getrc,
                LinkMessageHandler h, SignBufferFunc sign,
                SessionEstablishedHandler established,
                SessionRenegotiateHandler reneg, TimeoutHandler timeout,
                SessionClosedHandler closed, bool acceptInbound);

      /// destruct
      ~LinkLayer() override;

      /// get AI rank
      uint16_t
      Rank() const override;

      /// handle low level recv
      void
      RecvFrom(const Addr& from, const void* buf, size_t sz) override;

#ifdef __linux__
      /// process ICMP stuff on linux
      void
      ProcessICMP();
#endif

      /// pump sessions
      void
      Pump() override;

      /// stop link layer
      void
      Stop() override;

      /// regenerate transport keypair
      bool
      KeyGen(SecretKey& k) override;

      /// do tick
      void
      Tick(llarp_time_t now);

      /// create new outbound session
      std::shared_ptr< ILinkSession >
      NewOutboundSession(const RouterContact& rc,
                         const AddressInfo& addr) override;

      /// create new socket
      utp_socket*
      NewSocket();

      /// get ai name
      const char*
      Name() const override;
    };

  }  // namespace utp
}  // namespace llarp

#endif
