#ifndef LLARP_LINK_IWP_INTERNAL_HPP
#define LLARP_LINK_IWP_INTERNAL_HPP
#include <link/server.hpp>
#include <link/session.hpp>
#include <link_layer.hpp>

#include <bitset>
#include <deque>

namespace llarp
{
  namespace iwp
  {
    struct LinkLayer;

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

      void
      PumpIO();

      void
      TickIO(llarp_time_t now);

      bool
      QueueMessageBuffer(llarp_buffer_t buf);

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

      struct FragmentHeader
      {
        /// protocol version, always LLARP_PROTO_VERSION
        Proto_t version = LLARP_PROTO_VERSION;
        /// fragment command type
        Cmd_t cmd = 0;
        /// if cmd is XMIT this is the number of additional fragments this
        /// message has
        /// if cmd is FACK this is the fragment bitfield of the
        /// messages acked otherwise 0
        Flags_t flags = 0;
        /// if cmd is XMIT this is the fragment index
        /// if cmd is FACK this is set to 0xff to indicate message drop
        /// otherwise set to 0
        /// any other cmd it is set to 0
        Fragno_t fragno = 0;
        /// if cmd is XMIT then this is the size of the current fragment
        /// if cmd is FACK then this MUST be set to 0
        FragLen_t fraglen = 0;
        /// if cmd is XMIT or FACK this is the sequence number of the message
        /// otherwise it's 0
        Seqno_t seqno = 0;

        bool
        Decode(llarp_buffer_t *buf)
        {
          if(llarp_buffer_size_left(*buf) < fragoverhead)
            return false;
          version = *buf->cur;
          if(version != LLARP_PROTO_VERSION)
            return false;
          buf->cur++;
          cmd = *buf->cur;
          buf->cur++;
          flags = *buf->cur;
          buf->cur++;
          fragno = *buf->cur;
          buf->cur++;
          llarp_buffer_read_uint16(buf, &fraglen);
          llarp_buffer_read_uint32(buf, &seqno);
          return fraglen <= fragsize;
        }

        bool
        Encode(llarp_buffer_t *buf, llarp_buffer_t body)
        {
          if(body.sz > fragsize)
            return false;
          fraglen = body.sz;
          if(llarp_buffer_size_left(*buf) < (fragoverhead + fraglen))
            return false;
          *buf->cur = LLARP_PROTO_VERSION;
          buf->cur++;
          *buf->cur = cmd;
          buf->cur++;
          *buf->cur = flags;
          buf->cur++;
          *buf->cur = fragno;
          buf->cur++;
          llarp_buffer_put_uint16(buf, fraglen);
          llarp_buffer_put_uint32(buf, seqno);
          if(fraglen)
            memcpy(buf->cur, body.base, fraglen);
          buf->cur += fraglen;
          return true;
        }
      };

      struct MessageState
      {
        /// default
        MessageState(){};
        /// inbound
        MessageState(Flags_t numfrags)
        {
          acks.set();
          if(numfrags <= maxfrags)
          {
            while(numfrags)
              acks.reset(maxfrags - (numfrags--));
          }
          else  // invalid value
            return;
        }

        /// outbound
        MessageState(llarp_buffer_t buf)
        {
          sz = std::min(buf.sz, MAX_LINK_MSG_SIZE);
          memcpy(msg.data(), buf.base, sz);
          size_t idx = 0;
          acks.set();
          while(idx * fragsize < sz)
            acks.reset(idx++);
        };

        /// which fragments have we got
        std::bitset< maxfrags > acks;
        /// the message buffer
        MessageBuffer_t msg;
        /// the message's size
        FragLen_t sz;
        /// the last activity we have had
        llarp_time_t lastActiveAt;

        /// return true if this message is to be removed
        /// because of inactivity
        bool
        IsExpired(llarp_time_t now) const
        {
          return now > lastActiveAt && now - lastActiveAt > 2000;
        }

        bool
        IsDone() const
        {
          return acks.all();
        }

        bool
        ShouldRetransmit(llarp_time_t now) const
        {
          if(IsDone())
            return false;
          return now > lastActiveAt && now - lastActiveAt > 500;
        }

        template < typename write_pkt_func >
        bool
        TransmitUnacked(write_pkt_func write_pkt, Seqno_t seqno) const
        {
          static FragLen_t maxfragsize = fragsize;
          FragmentHeader hdr;
          hdr.seqno = seqno;
          hdr.cmd   = XMIT;
          AlignedBuffer< fragoverhead + fragsize > frag;
          auto buf          = frag.as_buffer();
          const byte_t *ptr = msg.data();
          Fragno_t idx      = 0;
          FragLen_t len     = sz;
          while(idx < maxfrags)
          {
            const FragLen_t l = std::min(len, maxfragsize);
            if(!acks.test(idx))
            {
              hdr.fragno  = idx;
              hdr.fraglen = l;
              if(!hdr.Encode(&buf, llarp::InitBuffer(ptr, l)))
                return false;
              buf.sz  = buf.cur - buf.base;
              buf.cur = buf.base;
              len -= l;
              if(write_pkt(buf.base, buf.sz) != int(buf.sz))
                return false;
            }
            ptr += l;
            len -= l;
            if(l >= fragsize)
              ++idx;
            else
              break;
          }
          return true;
        }

        template < typename write_pkt_func >
        bool
        TransmitAcks(write_pkt_func write_pkt, Seqno_t seqno)
        {
          FragmentHeader hdr;
          hdr.seqno  = seqno;
          hdr.cmd    = FACK;
          hdr.flags  = 0;
          byte_t idx = 0;
          while(idx < maxfrags)
          {
            if(acks.test(idx))
              hdr.flags |= 1 << idx;
            ++idx;
          }
          hdr.fraglen = 0;
          hdr.fragno  = 0;
          AlignedBuffer< fragoverhead > frag;
          auto buf = frag.as_buffer();
          if(!hdr.Encode(&buf, llarp::InitBuffer(nullptr, 0)))
            return false;
          return write_pkt(buf.base, buf.sz) == int(buf.sz);
        }
      };

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
                const SecretKey &identitySecretKey, llarp::GetRCFunc getrc,
                llarp::LinkMessageHandler h, llarp::SignBufferFunc sign,
                llarp::SessionEstablishedHandler established,
                llarp::SessionRenegotiateHandler reneg,
                llarp::TimeoutHandler timeout,
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

      const byte_t *
      IndentityKey() const
      {
        return m_IdentityKey.data();
      }

      const AlignedBuffer< 32 > &
      CookieSec() const
      {
        return m_CookieSec;
      }

      RouterID
      GetRouterID() const
      {
        return m_IdentityKey.toPublic();
      }

     private:
      bool
      SignBuffer(llarp::Signature &sig, llarp_buffer_t buf) const
      {
        return crypto->sign(sig, m_IdentityKey, buf);
      }
      const llarp::SecretKey m_IdentityKey;
      AlignedBuffer< 32 > m_CookieSec;

      /// handle ll recv
      void
      RecvFrom(const llarp::Addr &from, const void *buf, size_t sz) override;
    };
  }  // namespace iwp
}  // namespace llarp

#endif
