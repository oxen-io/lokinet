#include <llarp/routing/handler.hpp>
#include <llarp/service/protocol.hpp>
#include "buffer.hpp"
#include "mem.hpp"

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

    ProtocolMessage::~ProtocolMessage()
    {
    }

    void
    ProtocolMessage::PutBuffer(llarp_buffer_t buf)
    {
      payload.resize(buf.sz);
      memcpy(payload.data(), buf.base, buf.sz);
      payload.shrink_to_fit();
    }

    void
    ProtocolMessage::ProcessAsync(void* user)
    {
      ProtocolMessage* self = static_cast< ProtocolMessage* >(user);
      self->handler->HandleDataMessage(self);
      delete self;
    }

    bool
    ProtocolMessage::DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictInt("a", proto, read, k, buf))
        return false;
      if(llarp_buffer_eq(k, "d"))
      {
        llarp_buffer_t strbuf;
        if(!bencode_read_string(buf, &strbuf))
          return false;
        PutBuffer(strbuf);
        return true;
      }
      if(!BEncodeMaybeReadDictEntry("i", introReply, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("s", sender, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("t", tag, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("v", version, read, k, buf))
        return false;
      return read;
    }

    bool
    ProtocolMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictInt("a", proto, buf))
        return false;
      if(!bencode_write_bytestring(buf, "d", 1))
        return false;
      if(!bencode_write_bytestring(buf, payload.data(), payload.size()))
        return false;
      if(!BEncodeWriteDictEntry("i", introReply, buf))
        return false;
      if(!BEncodeWriteDictEntry("s", sender, buf))
        return false;
      if(!tag.IsZero())
      {
        if(!BEncodeWriteDictEntry("t", tag, buf))
          return false;
      }
      if(!bencode_write_version_entry(buf))
        return false;
      return bencode_end(buf);
    }

    ProtocolFrame::~ProtocolFrame()
    {
    }

    bool
    ProtocolFrame::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "H"))
        return false;
      if(!BEncodeWriteDictEntry("D", D, buf))
        return false;
      if(S == 0)
      {
        if(!BEncodeWriteDictEntry("H", H, buf))
          return false;
      }
      if(!BEncodeWriteDictEntry("N", N, buf))
        return false;
      if(!BEncodeWriteDictInt("S", S, buf))
        return false;
      if(S == 0)
      {
        if(!BEncodeWriteDictEntry("T", T, buf))
          return false;
      }
      if(!BEncodeWriteDictInt("V", version, buf))
        return false;
      if(!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    ProtocolFrame::DecodeKey(llarp_buffer_t key, llarp_buffer_t* val)
    {
      bool read = false;
      if(llarp_buffer_eq(key, "A"))
      {
        llarp_buffer_t strbuf;
        if(!bencode_read_string(val, &strbuf))
          return false;
        if(strbuf.sz != 1)
          return false;
        return *strbuf.cur == 'H';
      }
      if(!BEncodeMaybeReadDictEntry("D", D, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictEntry("H", H, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictEntry("N", N, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictInt("S", S, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictEntry("T", T, read, key, val))
        return false;
      if(!BEncodeMaybeReadVersion("V", version, LLARP_PROTO_VERSION, read, key,
                                  val))
        return false;
      if(!BEncodeMaybeReadDictEntry("Z", Z, read, key, val))
        return false;
      return read;
    }

    bool
    ProtocolFrame::DecryptPayloadInto(llarp_crypto* crypto, byte_t* sharedkey,
                                      ProtocolMessage* msg) const
    {
      auto buf = D.Buffer();
      crypto->xchacha20(buf, sharedkey, N);
      msg->PutBuffer(buf);
      return true;
    }

    bool
    ProtocolFrame::EncryptAndSign(llarp_crypto* crypto,
                                  const ProtocolMessage* msg,
                                  byte_t* sessionKey, byte_t* signingkey)
    {
      // put payload and encrypt
      D = llarp::ConstBuffer(msg->payload);
      memcpy(D.data(), msg->payload.data(), D.size());
      auto dbuf = D.Buffer();
      crypto->xchacha20(*dbuf, sessionKey, N);
      // zero out signature
      Z.Zero();
      // encode
      byte_t tmp[MAX_PROTOCOL_MESSAGE_SIZE];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(!BEncode(&buf))
        return false;
      // rewind
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      // sign
      return crypto->sign(Z, signingkey, buf);
    }

    struct AsyncFrameDH
    {
      llarp_crypto* crypto;
      llarp_logic* logic;
      ProtocolMessage* msg;
      byte_t* localSecret;
      PubKey H;
      KeyExchangeNonce N;
      IDataHandler* handler;
      Address remote;
      Encrypted D;

      AsyncFrameDH(llarp_logic* l, llarp_crypto* c, byte_t* sec,
                   IDataHandler* h, ProtocolMessage* m,
                   const ProtocolFrame* frame)
          : crypto(c)
          , logic(l)
          , msg(m)
          , localSecret(sec)
          , H(frame->H)
          , N(frame->N)
          , handler(h)
          , D(frame->D)
      {
      }

      static void
      Work(void* user)
      {
        AsyncFrameDH* self = static_cast< AsyncFrameDH* >(user);
        auto crypto        = self->crypto;
        SharedSecret shared;
        if(!crypto->dh_client(shared, self->H, self->localSecret, self->N))
        {
          llarp::LogError(
              "Failed to derive shared secret for initial message H=", self->H,
              " N=", self->N);
          delete self->msg;
          delete self;
          return;
        }
        auto buf = self->D.Buffer();
        crypto->xchacha20(*buf, shared, self->N);
        if(!self->msg->BDecode(buf))
        {
          llarp::LogError("failed to decode inner protocol message");
          llarp::DumpBuffer(*buf);
          delete self->msg;
          delete self;
          return;
        }
        self->handler->PutIntroFor(self->msg->tag, self->msg->introReply);
        self->handler->PutSenderFor(self->msg->tag, self->msg->sender);
        self->handler->PutCachedSessionKeyFor(self->msg->tag, shared);
        self->msg->handler = self->handler;
        llarp_logic_queue_job(self->logic,
                              {self->msg, &ProtocolMessage::ProcessAsync});
        delete self;
      }
    };

    bool
    ProtocolFrame::AsyncDecryptAndVerify(llarp_logic* logic,
                                         llarp_crypto* crypto,
                                         llarp_threadpool* worker,
                                         byte_t* localSecret,
                                         IDataHandler* handler) const
    {
      if(S == 0)
      {
        ProtocolMessage* msg = new ProtocolMessage();
        // we need to dh
        auto dh =
            new AsyncFrameDH(logic, crypto, localSecret, handler, msg, this);
        llarp_threadpool_queue_job(worker, {dh, &AsyncFrameDH::Work});
        return true;
      }
      SharedSecret shared;
      if(!handler->GetCachedSessionKeyFor(T, shared))
      {
        llarp::LogError("No cached session for T=", T);
        return false;
      }
      ServiceInfo si;
      if(!handler->GetSenderFor(T, si))
      {
        llarp::LogError("No sender for T=", T);
        return false;
      }
      if(!Verify(crypto, si))
      {
        llarp::LogError("Signature failure");
        return false;
      }
      ProtocolMessage* msg = new ProtocolMessage();
      if(!DecryptPayloadInto(crypto, shared, msg))
      {
        llarp::LogError("failed to decrypt message");
        delete msg;
        return false;
      }
      msg->handler = handler;
      llarp_logic_queue_job(logic, {msg, &ProtocolMessage::ProcessAsync});
      return true;
    }

    ProtocolFrame::ProtocolFrame()
    {
      T.Zero();
    }

    ProtocolFrame::ProtocolFrame(const ProtocolFrame& other)
        : D(other.D), H(other.H), N(other.N), Z(other.Z), T(other.T)
    {
    }

    bool
    ProtocolFrame::Verify(llarp_crypto* crypto, const ServiceInfo& from) const
    {
      ProtocolFrame copy(*this);
      // save signature
      // zero out signature for verify
      copy.Z.Zero();
      bool result = false;
      // serialize
      byte_t tmp[MAX_PROTOCOL_MESSAGE_SIZE];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(copy.BEncode(&buf))
      {
        // rewind buffer
        buf.sz  = buf.cur - buf.base;
        buf.cur = buf.base;
        // verify
        result = from.Verify(crypto, buf, Z);
      }
      // restore signature
      return result;
    }

    bool
    ProtocolFrame::HandleMessage(llarp::routing::IMessageHandler* h,
                                 llarp_router* r) const
    {
      llarp::LogInfo("Got hidden service frame");
      return h->HandleHiddenServiceFrame(this);
    }

  }  // namespace service
}  // namespace llarp