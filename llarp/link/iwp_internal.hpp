#ifndef LLARP_LINK_IWP_INTERNAL_HPP
#define LLARP_LINK_IWP_INTERNAL_HPP

#include <constants/link_layer.hpp>
#include <crypto/crypto.hpp>
#include <crypto/encrypted.hpp>
#include <crypto/types.hpp>
#include <link/server.hpp>
#include <link/session.hpp>

#include <bitset>
#include <deque>

namespace llarp
{
  struct Crypto;
  namespace iwp
  {
    struct LinkLayer;

    using FlowID_t = llarp::AlignedBuffer< 32 >;

    using OuterCommand_t = byte_t;

    constexpr OuterCommand_t eOCMD_ObtainFlowID     = 'O';
    constexpr OuterCommand_t eOCMD_GiveFlowID       = 'G';
    constexpr OuterCommand_t eOCMD_Reject           = 'R';
    constexpr OuterCommand_t eOCMD_SessionNegotiate = 'S';
    constexpr OuterCommand_t eOCMD_TransmitData     = 'D';

    using InnerCommand_t = byte_t;

    constexpr InnerCommand_t eICMD_KeepAlive       = 'k';
    constexpr InnerCommand_t eICMD_KeepAliveAck    = 'l';
    constexpr InnerCommand_t eICMD_Congestion      = 'c';
    constexpr InnerCommand_t eICMD_AntiCongestion  = 'd';
    constexpr InnerCommand_t eICMD_Transmit        = 't';
    constexpr InnerCommand_t eICMD_Ack             = 'a';
    constexpr InnerCommand_t eICMD_RotateKeys      = 'r';
    constexpr InnerCommand_t eICMD_UpgradeProtocol = 'u';
    constexpr InnerCommand_t eICMD_VersionUpgrade  = 'v';

    struct OuterMessage
    {
      // required memebers
      byte_t command;
      FlowID_t flow;

      // optional memebers follow
      NetID netid;
      FlowID_t nextFlowID;
      std::string rejectReason;
      AlignedBuffer< 24 > N;
      // TODO: compute optimal size
      AlignedBuffer< 1440 > X;
      size_t Xsize;
      ShortHash Z;

      /// encode to buffer
      bool
      Encode(llarp_buffer_t *buf) const;

      /// decode from buffer
      bool
      Decode(llarp_buffer_t *buf);

      /// verify signature if needed
      bool
      Verify(const SharedSecret &K) const;

      /// clear members
      void
      Clear();
    };

    /// TODO: fixme
    constexpr size_t MaxFrags = 8;

    using MessageBuffer_t = AlignedBuffer< MAX_LINK_MSG_SIZE >;
    using FragmentLen_t   = uint16_t;
    using SequenceNum_t   = uint32_t;

    using WritePacketFunc = std::function< void(const llarp_buffer_t &) >;

    struct MessageState
    {
      /// default
      MessageState();
      /// inbound
      MessageState(const ShortHash &digest, SequenceNum_t num);
      /// outbound
      MessageState(const ShortHash &digest, const llarp_buffer_t &buf,
                   SequenceNum_t num);

      /// the expected hash of the message
      const ShortHash expectedHash;

      /// which fragments have we got
      std::bitset< MaxFrags > acks;
      /// the message buffer
      MessageBuffer_t msg;
      /// the message's size
      FragmentLen_t sz;
      /// the last activity we have had
      llarp_time_t lastActiveAt;
      // sequence number
      const SequenceNum_t seqno;

      /// return true if this message is to be removed
      /// because of inactivity
      bool
      IsExpired(llarp_time_t now) const;

      /// return true if we have recvieved or sent the underlying message in
      /// full.
      bool
      IsDone() const;

      /// return true if we should retransmit some packets
      bool
      ShouldRetransmit(llarp_time_t now) const;

      /// transmit unacked fragments
      bool
      TransmitUnacked(WritePacketFunc write_pkt) const;

      /// transmit acks packet
      bool
      TransmitAcks(WritePacketFunc write_pkt);
    };

    struct Session final : public llarp::ILinkSession
    {
      /// base
      Session(LinkLayer *parent);
      /// inbound
      Session(LinkLayer *parent, const llarp::Addr &from);
      /// outbound
      Session(LinkLayer *parent, const RouterContact &rc,
              const AddressInfo &ai);
      ~Session();

      util::StatusObject
      ExtractStatus() const override
      {
        // TODO: fill me in.
        return {};
      }

      /// pump ll io
      void
      PumpIO();

      /// tick every 1 s
      void
      TickIO(llarp_time_t now);

      /// queue full message
      bool
      QueueMessageBuffer(const llarp_buffer_t &buf);

      /// return true if the session is established and handshaked and all that
      /// jazz
      bool
      SessionIsEstablished();

      /// inbound start
      void
      Accept();

      /// sendclose
      void
      Close();

      /// start outbound handshake
      void
      Connect();

      // set tls config
      void
      Configure();

      /// low level recv
      void
      Recv_ll(const void *buf, size_t sz);

      /// verify a lim
      bool
      VerfiyLIM(const llarp::LinkIntroMessage *msg);

      SharedSecret m_TXKey;
      SharedSecret m_RXKey;
      LinkLayer *m_Parent;
      llarp::Crypto *const crypto;
      llarp::RouterContact remoteRC;
      llarp::Addr remoteAddr;

      using MessageBuffer_t = llarp::AlignedBuffer< MAX_LINK_MSG_SIZE >;

      using Seqno_t   = uint32_t;
      using Proto_t   = uint8_t;
      using FragLen_t = uint16_t;
      using Flags_t   = uint8_t;
      using Fragno_t  = uint8_t;
      using Cmd_t     = uint8_t;

      static constexpr size_t fragoverhead = sizeof(Proto_t) + sizeof(Cmd_t)
          + sizeof(Flags_t) + sizeof(Fragno_t) + sizeof(FragLen_t)
          + sizeof(Seqno_t);

      /// keepalive command
      static constexpr Cmd_t PING = 0;
      /// transmit fragment command
      static constexpr Cmd_t XMIT = 1;
      /// fragment ack command
      static constexpr Cmd_t FACK = 2;

      /// maximum number of fragments
      static constexpr uint8_t maxfrags = 8;

      /// maximum fragment size
      static constexpr FragLen_t fragsize = MAX_LINK_MSG_SIZE / maxfrags;

      using MessageHolder_t = std::unordered_map< Seqno_t, MessageState >;

      MessageHolder_t m_Inbound;
      MessageHolder_t m_Outbound;

      using Buf_t     = std::vector< byte_t >;
      using IOQueue_t = std::deque< Buf_t >;

      IOQueue_t ll_recv;
      IOQueue_t ll_send;
    };

    struct LinkLayer final : public llarp::ILinkLayer
    {
      LinkLayer(llarp::Crypto *crypto, const SecretKey &encryptionSecretKey,
                llarp::GetRCFunc getrc, llarp::LinkMessageHandler h,
                llarp::SessionEstablishedHandler established,
                llarp::SessionRenegotiateHandler reneg,
                llarp::SignBufferFunc sign, llarp::TimeoutHandler timeout,
                llarp::SessionClosedHandler closed);

      ~LinkLayer();
      llarp::Crypto *const crypto;

      bool
      Start(llarp::Logic *l) override;

      ILinkSession *
      NewOutboundSession(const llarp::RouterContact &rc,
                         const llarp::AddressInfo &ai) override;

      void
      Pump() override;

      bool
      KeyGen(SecretKey &k) override;

      const char *
      Name() const override;

      uint16_t
      Rank() const override;

      /// verify that a new flow id matches addresses and old flow id
      bool
      VerifyFlowID(const FlowID_t &newID, const Addr &from,
                   const FlowID_t &oldID) const;

      void
      RecvFrom(const llarp::Addr &from, const void *buf, size_t sz) override;

     private:
      bool
      ShouldSendFlowID(const Addr &from) const;

      void
      SendReject(const Addr &to, const FlowID_t &flow, const char *msg);

      void
      SendFlowID(const Addr &to, const FlowID_t &flow);

      using ActiveFlows_t =
          std::unordered_map< FlowID_t, RouterID, FlowID_t::Hash >;

      ActiveFlows_t m_ActiveFlows;

      using PendingFlows_t = std::unordered_map< Addr, FlowID_t, Addr::Hash >;
      /// flows that are pending authentication
      PendingFlows_t m_PendingFlows;

      /// cookie used in flow id computation
      AlignedBuffer< 32 > m_FlowCookie;

      OuterMessage m_OuterMsg;
    };
  }  // namespace iwp
}  // namespace llarp

#endif
