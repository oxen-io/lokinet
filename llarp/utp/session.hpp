#ifndef LLARP_UTP_SESSION_HPP
#define LLARP_UTP_SESSION_HPP

#include <crypto/crypto.hpp>
#include <link/session.hpp>
#include <utp/inbound_message.hpp>
#include <deque>

#include <utp.h>

namespace llarp
{
  namespace utp
  {
    struct LinkLayer;

    struct Session : public ILinkSession
    {
      /// remote router's rc
      RouterContact remoteRC;
      /// underlying socket
      utp_socket* sock;
      /// link layer parent
      ILinkLayer* parent;
      /// did we get a LIM from the remote yet?
      bool gotLIM;
      /// remote router's transport pubkey
      PubKey remoteTransportPubKey;
      /// remote router's transport ip
      Addr remoteAddr;
      /// rx session key
      SharedSecret rxKey;
      /// tx session key
      SharedSecret txKey;
      /// timestamp last active
      llarp_time_t lastActive = 0;
      /// timestamp last send success
      llarp_time_t lastSend = 0;
      /// session timeout (60s)
      const static llarp_time_t sessionTimeout = DefaultLinkSessionLifetime;

      struct OutboundMessage
      {
        OutboundMessage(uint32_t id, CompletionHandler func)
            : msgid{id}, completed{func}
        {
        }

        const uint32_t msgid;
        std::deque< utp_iovec > vecs;
        std::deque< FragmentBuffer > fragments;
        CompletionHandler completed;

        void
        Dropped()
        {
          if(completed)
          {
            completed(DeliveryStatus::eDeliveryDropped);
            completed = nullptr;
          }
        }

        void
        Delivered()
        {
          if(completed)
          {
            completed(DeliveryStatus::eDeliverySuccess);
            completed = nullptr;
          }
        }

        bool
        operator<(const OutboundMessage& other) const
        {
          return msgid < other.msgid;
        }
      };

      /// current rx fragment buffer
      FragmentBuffer recvBuf;
      /// current offset in current rx fragment buffer
      size_t recvBufOffset;
      /// rx fragment message body
      AlignedBuffer< FragmentBodySize > rxFragBody;

      /// the next message id for tx
      uint32_t m_NextTXMsgID;
      /// the next message id for rx
      uint32_t m_NextRXMsgID;

      using SendQueue_t = std::deque< OutboundMessage >;
      /// messages we are currently sending
      SendQueue_t sendq;
      /// messages we are recving right now
      std::unordered_map< uint32_t, InboundMessage > m_RecvMsgs;
      /// are we stalled or nah?
      bool stalled = false;

      uint64_t m_RXRate = 0;
      uint64_t m_TXRate = 0;

      /// mark session as alive
      void
      Alive();

      util::StatusObject
      ExtractStatus() const override;

      virtual ~Session() = 0;

      /// base
      explicit Session(LinkLayer* p);

      enum State
      {
        eInitial,          // initial state
        eConnecting,       // we are connecting
        eLinkEstablished,  // when utp connection is established
        eCryptoHandshake,  // crypto handshake initiated
        eSessionReady,     // session is ready
        eClose             // utp connection is closed
      };

      /// session state, call EnterState(State) to set
      State state;

      /// hook for utp for when we have established a connection
      void
      OnLinkEstablished(ILinkLayer* p) override;

      /// switch states
      void
      EnterState(State st);

      /// handle LIM after handshake
      bool
      GotSessionRenegotiate(const LinkIntroMessage* msg);

      /// re negotiate session with our new local RC
      bool
      RenegotiateSession() override;

      bool
      ShouldPing() const override;

      /// pump tx queue
      void
      PumpWrite();

      void
      Pump() override;

      bool
      SendKeepAlive() override;

      bool
      IsEstablished() const override
      {
        return state == eSessionReady;
      }

      bool
      TimedOut(llarp_time_t now) const override;

      /// verify a fragment buffer and the decrypt it
      /// buf is assumed to be FragmentBufferSize bytes long
      bool
      VerifyThenDecrypt(const byte_t* buf);

      /// encrypt a fragment then hash the ciphertext
      bool
      EncryptThenHash(const byte_t* ptr, uint32_t msgid, uint16_t sz,
                      uint16_t remain);

      /// queue a fully formed message
      bool
      SendMessageBuffer(const llarp_buffer_t& buf,
                        ILinkSession::CompletionHandler) override;

      /// prune expired inbound messages
      void
      PruneInboundMessages(llarp_time_t now);

      /// do low level connect
      void
      Connect();

      /// handle outbound connection made
      void
      OutboundLinkEstablished(LinkLayer* p);

      // send first message
      void
      OutboundHandshake();

      // do key exchange for handshake
      template < bool (Crypto::*dh_func)(SharedSecret&, const PubKey&,
                                         const SecretKey&, const TunnelNonce&) >
      bool
      DoKeyExchange(SharedSecret& K, const KeyExchangeNonce& n,
                    const PubKey& other, const SecretKey& secret);

      bool
      DoClientKeyExchange(SharedSecret& K, const KeyExchangeNonce& n,
                          const PubKey& other, const SecretKey& secret)
      {
        return DoKeyExchange< &Crypto::transport_dh_client >(K, n, other,
                                                             secret);
      }

      bool
      DoServerKeyExchange(SharedSecret& K, const KeyExchangeNonce& n,
                          const PubKey& other, const SecretKey& secret)
      {
        return DoKeyExchange< &Crypto::transport_dh_server >(K, n, other,
                                                             secret);
      }

      /// does K = HS(K + A)
      bool
      MutateKey(SharedSecret& K, const AlignedBuffer< 24 >& A);

      void
      Tick(llarp_time_t now) override;

      /// close session
      void
      Close() override;

      /// low level read
      bool
      Recv(const byte_t* buf, size_t sz);

      /// get remote identity pubkey
      PubKey
      GetPubKey() const override;

      /// get remote address
      Addr
      GetRemoteEndpoint() const override;

      RouterContact
      GetRemoteRC() const override
      {
        return remoteRC;
      }

      /// get parent link
      ILinkLayer*
      GetLinkLayer() const override;

      void
      MarkEstablished();

      size_t
      SendQueueBacklog() const override
      {
        return sendq.size();
      }
    };

    struct InboundSession final : public Session
    {
      InboundSession(LinkLayer* p, utp_socket* s, const Addr& addr);

      bool
      InboundLIM(const LinkIntroMessage* msg);

      void
      Start() override
      {
      }
    };

    struct OutboundSession final : public Session
    {
      OutboundSession(LinkLayer* p, utp_socket* s, const RouterContact& rc,
                      const AddressInfo& addr);

      bool
      OutboundLIM(const LinkIntroMessage* msg);

      void
      Start() override;
    };
  }  // namespace utp
}  // namespace llarp

#endif
