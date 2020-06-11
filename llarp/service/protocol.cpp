#include <service/protocol.hpp>
#include <path/path.hpp>
#include <routing/handler.hpp>
#include <util/buffer.hpp>
#include <util/mem.hpp>
#include <util/meta/memfn.hpp>
#include <util/thread/logic.hpp>
#include <service/endpoint.hpp>
#include <router/abstractrouter.hpp>
#include <utility>

namespace llarp
{
  namespace service
  {
    ProtocolMessage::ProtocolMessage()
    {
      tag.Zero();
    }

    ProtocolMessage::ProtocolMessage(const ConvoTag& t) : tag(t)
    {
    }

    ProtocolMessage::~ProtocolMessage() = default;

    void
    ProtocolMessage::PutBuffer(const llarp_buffer_t& buf)
    {
      payload.resize(buf.sz);
      memcpy(payload.data(), buf.base, buf.sz);
    }

    void
    ProtocolMessage::ProcessAsync(
        path::Path_ptr path, PathID_t from, std::shared_ptr<ProtocolMessage> self)
    {
      if (!self->handler->HandleDataMessage(path, from, self))
        LogWarn("failed to handle data message from ", path->Name());
    }

    bool
    ProtocolMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictInt("a", proto, read, k, buf))
        return false;
      if (k == "d")
      {
        llarp_buffer_t strbuf;
        if (!bencode_read_string(buf, &strbuf))
          return false;
        PutBuffer(strbuf);
        return true;
      }
      if (!BEncodeMaybeReadDictEntry("i", introReply, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("n", seqno, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("s", sender, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("t", tag, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("v", version, read, k, buf))
        return false;
      return read;
    }

    bool
    ProtocolMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictInt("a", proto, buf))
        return false;
      if (!bencode_write_bytestring(buf, "d", 1))
        return false;
      if (!bencode_write_bytestring(buf, payload.data(), payload.size()))
        return false;
      if (!BEncodeWriteDictEntry("i", introReply, buf))
        return false;
      if (!BEncodeWriteDictInt("n", seqno, buf))
        return false;
      if (!BEncodeWriteDictEntry("s", sender, buf))
        return false;
      if (!tag.IsZero())
      {
        if (!BEncodeWriteDictEntry("t", tag, buf))
          return false;
      }
      if (!BEncodeWriteDictInt("v", version, buf))
        return false;
      return bencode_end(buf);
    }

    ProtocolFrame::~ProtocolFrame() = default;

    bool
    ProtocolFrame::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;

      if (!BEncodeWriteDictMsgType(buf, "A", "H"))
        return false;
      if (!C.IsZero())
      {
        if (!BEncodeWriteDictEntry("C", C, buf))
          return false;
      }
      if (D.size() > 0)
      {
        if (!BEncodeWriteDictEntry("D", D, buf))
          return false;
      }
      if (!BEncodeWriteDictEntry("F", F, buf))
        return false;
      if (!N.IsZero())
      {
        if (!BEncodeWriteDictEntry("N", N, buf))
          return false;
      }
      if (R)
      {
        if (!BEncodeWriteDictInt("R", R, buf))
          return false;
      }
      if (!T.IsZero())
      {
        if (!BEncodeWriteDictEntry("T", T, buf))
          return false;
      }
      if (!BEncodeWriteDictInt("V", version, buf))
        return false;
      if (!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    ProtocolFrame::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
    {
      bool read = false;
      if (key == "A")
      {
        llarp_buffer_t strbuf;
        if (!bencode_read_string(val, &strbuf))
          return false;
        if (strbuf.sz != 1)
          return false;
        return *strbuf.cur == 'H';
      }
      if (!BEncodeMaybeReadDictEntry("D", D, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("F", F, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("C", C, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("N", N, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictInt("S", S, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictInt("R", R, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("T", T, read, key, val))
        return false;
      if (!BEncodeMaybeVerifyVersion("V", version, LLARP_PROTO_VERSION, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("Z", Z, read, key, val))
        return false;
      return read;
    }

    bool
    ProtocolFrame::DecryptPayloadInto(const SharedSecret& sharedkey, ProtocolMessage& msg) const
    {
      Encrypted_t tmp = D;
      auto buf = tmp.Buffer();
      CryptoManager::instance()->xchacha20(*buf, sharedkey, N);
      return bencode_decode_dict(msg, buf);
    }

    bool
    ProtocolFrame::Sign(const Identity& localIdent)
    {
      Z.Zero();
      std::array<byte_t, MAX_PROTOCOL_MESSAGE_SIZE> tmp;
      llarp_buffer_t buf(tmp);
      // encode
      if (!BEncode(&buf))
      {
        LogError("message too big to encode");
        return false;
      }
      // rewind
      buf.sz = buf.cur - buf.base;
      buf.cur = buf.base;
      // sign
      return localIdent.Sign(Z, buf);
    }

    bool
    ProtocolFrame::EncryptAndSign(
        const ProtocolMessage& msg, const SharedSecret& sessionKey, const Identity& localIdent)
    {
      std::array<byte_t, MAX_PROTOCOL_MESSAGE_SIZE> tmp;
      llarp_buffer_t buf(tmp);
      // encode message
      if (!msg.BEncode(&buf))
      {
        LogError("message too big to encode");
        return false;
      }
      // rewind
      buf.sz = buf.cur - buf.base;
      buf.cur = buf.base;
      // encrypt
      CryptoManager::instance()->xchacha20(buf, sessionKey, N);
      // put encrypted buffer
      D = buf;
      // zero out signature
      Z.Zero();
      llarp_buffer_t buf2(tmp);
      // encode frame
      if (!BEncode(&buf2))
      {
        LogError("frame too big to encode");
        DumpBuffer(buf2);
        return false;
      }
      // rewind
      buf2.sz = buf2.cur - buf2.base;
      buf2.cur = buf2.base;
      // sign
      if (!localIdent.Sign(Z, buf2))
      {
        LogError("failed to sign? wtf?!");
        return false;
      }
      return true;
    }

    struct AsyncFrameDecrypt
    {
      path::Path_ptr path;
      std::shared_ptr<Logic> logic;
      std::shared_ptr<ProtocolMessage> msg;
      const Identity& m_LocalIdentity;
      Endpoint* handler;
      const ProtocolFrame frame;
      const Introduction fromIntro;

      AsyncFrameDecrypt(
          std::shared_ptr<Logic> l,
          const Identity& localIdent,
          Endpoint* h,
          std::shared_ptr<ProtocolMessage> m,
          const ProtocolFrame& f,
          const Introduction& recvIntro)
          : logic(std::move(l))
          , msg(std::move(m))
          , m_LocalIdentity(localIdent)
          , handler(h)
          , frame(f)
          , fromIntro(recvIntro)
      {
      }

      static void
      Work(std::shared_ptr<AsyncFrameDecrypt> self)
      {
        auto crypto = CryptoManager::instance();
        SharedSecret K;
        SharedSecret sharedKey;
        // copy
        ProtocolFrame frame(self->frame);
        if (!crypto->pqe_decrypt(self->frame.C, K, pq_keypair_to_secret(self->m_LocalIdentity.pq)))
        {
          LogError("pqke failed C=", self->frame.C);
          self->msg.reset();
          return;
        }
        // decrypt
        auto buf = frame.D.Buffer();
        crypto->xchacha20(*buf, K, self->frame.N);
        if (!bencode_decode_dict(*self->msg, buf))
        {
          LogError("failed to decode inner protocol message");
          DumpBuffer(*buf);
          self->msg.reset();
          return;
        }
        // verify signature of outer message after we parsed the inner message
        if (!self->frame.Verify(self->msg->sender))
        {
          LogError(
              "intro frame has invalid signature Z=",
              self->frame.Z,
              " from ",
              self->msg->sender.Addr());
          Dump<MAX_PROTOCOL_MESSAGE_SIZE>(self->frame);
          Dump<MAX_PROTOCOL_MESSAGE_SIZE>(*self->msg);
          self->msg.reset();
          return;
        }

        if (self->handler->HasConvoTag(self->msg->tag))
        {
          LogError("dropping duplicate convo tag T=", self->msg->tag);
          // TODO: send convotag reset
          self->msg.reset();
          return;
        }

        // PKE (A, B, N)
        SharedSecret sharedSecret;
        path_dh_func dh_server = util::memFn(&Crypto::dh_server, CryptoManager::instance());

        if (!self->m_LocalIdentity.KeyExchange(
                dh_server, sharedSecret, self->msg->sender, self->frame.N))
        {
          LogError("x25519 key exchange failed");
          Dump<MAX_PROTOCOL_MESSAGE_SIZE>(self->frame);
          self->msg.reset();
          return;
        }
        std::array<byte_t, 64> tmp;
        // K
        std::copy(K.begin(), K.end(), tmp.begin());
        // S = HS( K + PKE( A, B, N))
        std::copy(sharedSecret.begin(), sharedSecret.end(), tmp.begin() + 32);
        crypto->shorthash(sharedKey, llarp_buffer_t(tmp));

        std::shared_ptr<ProtocolMessage> msg = std::move(self->msg);
        path::Path_ptr path = std::move(self->path);
        const PathID_t from = self->frame.F;
        msg->handler = self->handler;

        self->handler->AsyncAuthConvoTag(
            msg->sender.Addr(),
            msg->tag,
            [path, msg, from, handler = self->handler, fromIntro = self->fromIntro, sharedKey](
                AuthResult result) {
              if (result == AuthResult::eAuthAccepted)
              {
                LogInfo("Accepted Convo T=", msg->tag);
                handler->PutIntroFor(msg->tag, msg->introReply);
                handler->PutReplyIntroFor(msg->tag, fromIntro);
                handler->PutSenderFor(msg->tag, msg->sender, true);
                handler->PutCachedSessionKeyFor(msg->tag, sharedKey);
                ProtocolMessage::ProcessAsync(path, from, msg);
              }
              else
              {
                LogInfo("Rejected Convo T=", msg->tag);
                handler->SendAuthReject(path, from, msg->tag, result);
              }
            });
      }
    };

    ProtocolFrame&
    ProtocolFrame::operator=(const ProtocolFrame& other)
    {
      C = other.C;
      D = other.D;
      F = other.F;
      N = other.N;
      Z = other.Z;
      T = other.T;
      R = other.R;
      S = other.S;
      version = other.version;
      return *this;
    }

    struct AsyncDecrypt
    {
      ServiceInfo si;
      SharedSecret shared;
      ProtocolFrame frame;
    };

    bool
    ProtocolFrame::AsyncDecryptAndVerify(
        std::shared_ptr<Logic> logic,
        path::Path_ptr recvPath,
        const Identity& localIdent,
        Endpoint* handler) const
    {
      auto msg = std::make_shared<ProtocolMessage>();
      msg->handler = handler;
      if (T.IsZero())
      {
        LogInfo("Got protocol frame with new convo");
        // we need to dh
        auto dh = std::make_shared<AsyncFrameDecrypt>(
            logic, localIdent, handler, msg, *this, recvPath->intro);
        dh->path = recvPath;
        handler->Router()->QueueWork(std::bind(&AsyncFrameDecrypt::Work, dh));
        return true;
      }

      auto v = std::make_shared<AsyncDecrypt>();

      if (!handler->GetCachedSessionKeyFor(T, v->shared))
      {
        LogError("No cached session for T=", T);
        return true;
      }

      if (!handler->GetSenderFor(T, v->si))
      {
        LogError("No sender for T=", T);
        return false;
      }
      v->frame = *this;
      handler->Router()->QueueWork([v, msg = std::move(msg), recvPath = std::move(recvPath)]() {
        if (not v->frame.Verify(v->si))
        {
          LogError("Signature failure from ", v->si.Addr());
          return;
        }
        if (not v->frame.DecryptPayloadInto(v->shared, *msg))
        {
          LogError("failed to decrypt message");
          return;
        }
        RecvDataEvent ev;
        ev.fromPath = std::move(recvPath);
        ev.pathid = v->frame.F;
        ev.msg = std::move(msg);
        msg->handler->QueueRecvData(std::move(ev));
      });
      return true;
    }

    bool
    ProtocolFrame::operator==(const ProtocolFrame& other) const
    {
      return C == other.C && D == other.D && N == other.N && Z == other.Z && T == other.T
          && S == other.S && version == other.version;
    }

    bool
    ProtocolFrame::Verify(const ServiceInfo& svc) const
    {
      ProtocolFrame copy(*this);
      // save signature
      // zero out signature for verify
      copy.Z.Zero();
      // serialize
      std::array<byte_t, MAX_PROTOCOL_MESSAGE_SIZE> tmp;
      llarp_buffer_t buf(tmp);
      if (!copy.BEncode(&buf))
      {
        LogError("bencode fail");
        return false;
      }

      // rewind buffer
      buf.sz = buf.cur - buf.base;
      buf.cur = buf.base;
      // verify
      return svc.Verify(buf, Z);
    }

    bool
    ProtocolFrame::HandleMessage(routing::IMessageHandler* h, AbstractRouter* /*r*/) const
    {
      return h->HandleHiddenServiceFrame(*this);
    }

  }  // namespace service
}  // namespace llarp
