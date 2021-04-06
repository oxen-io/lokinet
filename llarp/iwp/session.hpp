#pragma once

#include <llarp/link/session.hpp>
#include "linklayer.hpp"
#include "message_buffer.hpp"
#include <llarp/net/ip_address.hpp>

#include <map>
#include <unordered_set>
#include <deque>
#include <queue>

#include <llarp/util/thread/queue.hpp>

namespace llarp
{
  namespace iwp
  {
    /// packet crypto overhead size
    static constexpr size_t PacketOverhead = HMACSIZE + TUNNONCESIZE;
    /// creates a packet with plaintext size + wire overhead + random pad
    ILinkSession::Packet_t
    CreatePacket(Command cmd, size_t plainsize, size_t min_pad = 16, size_t pad_variance = 16);
    /// Time how long we try delivery for
    static constexpr std::chrono::milliseconds DeliveryTimeout = 500ms;
    /// Time how long we wait to recieve a message
    static constexpr auto ReceivalTimeout = (DeliveryTimeout * 8) / 5;
    /// How long to keep a replay window for
    static constexpr auto ReplayWindow = (ReceivalTimeout * 3) / 2;
    /// How often to acks RX messages
    static constexpr auto ACKResendInterval = DeliveryTimeout / 2;
    /// How often to retransmit TX fragments
    static constexpr auto TXFlushInterval = (DeliveryTimeout / 5) * 4;
    /// How often we send a keepalive
    static constexpr std::chrono::milliseconds PingInterval = 5s;
    /// How long we wait for a session to die with no tx from them
    static constexpr auto SessionAliveTimeout = PingInterval * 5;

    struct Session : public ILinkSession, public std::enable_shared_from_this<Session>
    {
      using Time_t = std::chrono::milliseconds;

      /// maximum number of messages we can ack in a multiack
      static constexpr std::size_t MaxACKSInMACK = 1024 / sizeof(uint64_t);

      /// outbound session
      Session(LinkLayer* parent, const RouterContact& rc, const AddressInfo& ai);
      /// inbound session
      Session(LinkLayer* parent, const SockAddr& from);

      ~Session() = default;

      void
      Pump() override;

      void
      Tick(llarp_time_t now) override;

      bool
      SendMessageBuffer(ILinkSession::Message_t msg, CompletionHandler resultHandler) override;

      void
      Send_LL(const byte_t* buf, size_t sz);

      void EncryptAndSend(ILinkSession::Packet_t);

      void
      Start() override;

      void
      Close() override;

      bool Recv_LL(ILinkSession::Packet_t) override;

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

      const SockAddr&
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

      SessionStats
      GetSessionStats() const override;

      util::StatusObject
      ExtractStatus() const override;

      bool
      IsInbound() const override
      {
        return m_Inbound;
      }
      void
      HandlePlaintext();

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
      static std::string
      StateToString(State state);
      State m_State;
      SessionStats m_Stats;

      /// are we inbound session ?
      const bool m_Inbound;
      /// parent link layer
      LinkLayer* const m_Parent;
      const llarp_time_t m_CreatedAt;
      const SockAddr m_RemoteAddr;

      AddressInfo m_ChosenAI;
      /// remote rc
      RouterContact m_RemoteRC;
      /// session key
      SharedSecret m_SessionKey;
      /// session token
      AlignedBuffer<24> token;

      PubKey m_ExpectedIdent;
      PubKey m_RemoteOnionKey;

      llarp_time_t m_LastTX = 0s;
      llarp_time_t m_LastRX = 0s;

      // accumulate for periodic rate calculation
      uint64_t m_TXRate = 0;
      uint64_t m_RXRate = 0;

      llarp_time_t m_ResetRatesAt = 0s;

      uint64_t m_TXID = 0;

      bool
      ShouldResetRates(llarp_time_t now) const;

      void
      ResetRates();

      std::map<uint64_t, InboundMessage> m_RXMsgs;
      std::map<uint64_t, OutboundMessage> m_TXMsgs;

      /// maps rxid to time recieved
      std::unordered_map<uint64_t, llarp_time_t> m_ReplayFilter;
      /// rx messages to send in next round of multiacks
      std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>> m_SendMACKs;

      using CryptoQueue_t = std::vector<Packet_t>;

      CryptoQueue_t m_EncryptNext;
      CryptoQueue_t m_DecryptNext;

      llarp::thread::Queue<CryptoQueue_t> m_PlaintextRecv;

      void
      EncryptWorker(CryptoQueue_t msgs);

      void
      DecryptWorker(CryptoQueue_t msgs);

      void
      HandleGotIntro(Packet_t pkt);

      void
      HandleGotIntroAck(Packet_t pkt);

      void
      HandleCreateSessionRequest(Packet_t pkt);

      void
      HandleAckSession(Packet_t pkt);

      void
      HandleSessionData(Packet_t pkt);

      bool
      DecryptMessageInPlace(Packet_t& pkt);

      void
      SendMACK();

      void
      HandleRecvMsgCompleted(const InboundMessage& msg);

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
      HandleXMIT(Packet_t msg);

      void
      HandleDATA(Packet_t msg);

      void
      HandleACKS(Packet_t msg);

      void
      HandleNACK(Packet_t msg);

      void
      HandlePING(Packet_t msg);

      void
      HandleCLOS(Packet_t msg);

      void
      HandleMACK(Packet_t msg);
    };
  }  // namespace iwp
}  // namespace llarp
