#ifndef LLARP_IWP_SESSION_HPP
#define LLARP_IWP_SESSION_HPP

#include <link/session.hpp>
#include <iwp/linklayer.hpp>
#include <iwp/message_buffer.hpp>

namespace llarp
{
  namespace iwp
  {
    void
    AddRandomPadding(std::vector< byte_t >& pkt, size_t min = 16,
                     size_t variance = 16);

    struct Session : public ILinkSession,
                     public std::enable_shared_from_this< Session >
    {
      /// Time how long we try delivery for
      static constexpr llarp_time_t DeliveryTimeout = 5000;
      /// How long to keep a replay window for
      static constexpr llarp_time_t ReplayWindow = (DeliveryTimeout * 3) / 2;
      /// Time how long we wait to recieve a message
      static constexpr llarp_time_t RecievalTimeout = (DeliveryTimeout * 8) / 5;
      /// How often to acks RX messages
      static constexpr llarp_time_t ACKResendInterval = 500;
      /// How often to retransmit TX fragments
      static constexpr llarp_time_t TXFlushInterval =
          (ACKResendInterval * 3) / 2;
      /// How often we send a keepalive
      static constexpr llarp_time_t PingInterval = 2000;
      /// How long we wait for a session to die with no tx from them
      static constexpr llarp_time_t SessionAliveTimeout =
          (PingInterval * 13) / 3;

      /// outbound session
      Session(LinkLayer* parent, RouterContact rc, AddressInfo ai);
      /// inbound session
      Session(LinkLayer* parent, Addr from);

      ~Session() = default;

      void
      Pump() override;

      void
      Tick(llarp_time_t now) override;

      bool
      SendMessageBuffer(const llarp_buffer_t& buf,
                        CompletionHandler resultHandler) override;

      void
      Send_LL(const llarp_buffer_t& pkt);

      void
      EncryptAndSend(const llarp_buffer_t& data);

      void
      Start() override;

      void
      Close() override;

      void
      Recv_LL(const llarp_buffer_t& pkt) override;

      bool
      SendKeepAlive() override;

      bool
      IsEstablished() const override;

      bool
      TimedOut(llarp_time_t now) const override;

      PubKey
      GetPubKey() const override
      {
        return m_RemoteRC.pubkey;
      }

      Addr
      GetRemoteEndpoint() const override
      {
        return m_RemoteAddr;
      }

      RouterContact
      GetRemoteRC() const override
      {
        return m_RemoteRC;
      }

      size_t
      SendQueueBacklog() const override
      {
        return m_TXMsgs.size();
      }

      ILinkLayer*
      GetLinkLayer() const override
      {
        return m_Parent;
      }

      bool
      RenegotiateSession() override;

      bool
      ShouldPing() const override;

      util::StatusObject
      ExtractStatus() const override;

     private:
      enum class State
      {
        /// we have no data recv'd
        Initial,
        /// we are in introduction phase
        Introduction,
        /// we sent our LIM
        LinkIntro,
        /// handshake done and LIM has been obtained
        Ready,
        /// we are closed now
        Closed
      };
      State m_State;
      /// are we inbound session ?
      const bool m_Inbound;
      /// parent link layer
      LinkLayer* const m_Parent;
      const llarp_time_t m_CreatedAt;
      const Addr m_RemoteAddr;

      AddressInfo m_ChosenAI;
      /// remote rc
      RouterContact m_RemoteRC;
      /// session key
      SharedSecret m_SessionKey;
      /// session token
      AlignedBuffer< 24 > token;

      PubKey m_RemoteOnionKey;

      llarp_time_t m_LastTX = 0;
      llarp_time_t m_LastRX = 0;

      uint64_t m_TXID = 0;

      std::unordered_map< uint64_t, InboundMessage > m_RXMsgs;
      std::unordered_map< uint64_t, OutboundMessage > m_TXMsgs;

      /// maps rxid to time recieved
      std::unordered_map< uint64_t, llarp_time_t > m_ReplayFilter;

      void
      HandleGotIntro(const llarp_buffer_t& buf);

      void
      HandleGotIntroAck(const llarp_buffer_t& buf);

      void
      HandleCreateSessionRequest(const llarp_buffer_t& buf);

      void
      HandleAckSession(const llarp_buffer_t& buf);

      void
      HandleSessionData(const llarp_buffer_t& buf);

      bool
      DecryptMessage(const llarp_buffer_t& buf, std::vector< byte_t >& result);

      void
      GenerateAndSendIntro();

      bool
      GotInboundLIM(const LinkIntroMessage* msg);

      bool
      GotOutboundLIM(const LinkIntroMessage* msg);

      bool
      GotRenegLIM(const LinkIntroMessage* msg);

      void
      SendOurLIM(ILinkSession::CompletionHandler h = nullptr);

      void
      HandleXMIT(std::vector< byte_t > msg);

      void
      HandleDATA(std::vector< byte_t > msg);

      void
      HandleACKS(std::vector< byte_t > msg);

      void
      HandleNACK(std::vector< byte_t > msg);

      void
      HandlePING(std::vector< byte_t > msg);

      void
      HandleCLOS(std::vector< byte_t > msg);
    };
  }  // namespace iwp
}  // namespace llarp

#endif