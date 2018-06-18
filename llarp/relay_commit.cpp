#include <llarp/bencode.hpp>
#include <llarp/messages/relay_commit.hpp>
#include "logger.hpp"
#include "router.hpp"

namespace llarp
{
  bool
  LR_AcceptRecord::BEncode(llarp_buffer_t* buf) const
  {
    if(!bencode_start_dict(buf))
      return false;
    if(!BEncodeWriteDictMsgType(buf, "c", "a"))
      return false;
    if(!BEncodeWriteDictEntry("p", pathid, buf))
      return false;
    if(!BEncodeWriteDictEntry("r", downstream, buf))
      return false;
    if(!BEncodeWriteDictEntry("t", upstream, buf))
      return false;
    if(!BEncodeWriteDictInt(buf, "v", LLARP_PROTO_VERSION))
      return false;
    return bencode_end(buf);
  }

  LR_CommitMessage::~LR_CommitMessage()
  {
  }

  bool
  LR_CommitMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
  {
    if(llarp_buffer_eq(key, "c"))
    {
      return BEncodeReadList(frames, buf);
    }
    bool read = false;
    if(!BEncodeMaybeReadVersion("v", version, LLARP_PROTO_VERSION, read, key,
                                buf))
      return false;

    return read;
  }

  bool
  LR_CommitMessage::BEncode(llarp_buffer_t* buf) const
  {
    if(!bencode_start_dict(buf))
      return false;
    // msg type
    if(!BEncodeWriteDictMsgType(buf, "a", "c"))
      return false;
    // frames
    if(!BEncodeWriteDictList("c", frames, buf))
      return false;
    // version
    if(!bencode_write_version_entry(buf))
      return false;

    return bencode_end(buf);
  }

  bool
  LR_CommitMessage::HandleMessage(llarp_router* router) const
  {
    if(frames.size() != MAXHOPS)
    {
      llarp::Error("LRCM invalid number of records, ", frames.size(),
                   "!=", MAXHOPS);
      return false;
    }
    if(!router->paths.AllowingTransit())
    {
      llarp::Error("got an LRCM from ", remote,
                   " when we are not allowing transit");
      return false;
    }
    return AsyncDecrypt(&router->paths);
  }

  bool
  LR_CommitRecord::BEncode(llarp_buffer_t* buf) const
  {
    if(!bencode_start_dict(buf))
      return false;

    if(!BEncodeWriteDictEntry("c", commkey, buf))
      return false;
    if(!BEncodeWriteDictEntry("i", nextHop, buf))
      return false;
    if(!BEncodeWriteDictEntry("n", tunnelNonce, buf))
      return false;
    if(!BEncodeWriteDictEntry("p", pathid, buf))
      return false;
    if(!bencode_write_version_entry(buf))
      return false;
    if(work && !BEncodeWriteDictEntry("w", *work, buf))
      return false;

    return bencode_end(buf);
  }

  LR_CommitRecord::~LR_CommitRecord()
  {
    if(work)
      delete work;
  }

  bool
  LR_CommitRecord::OnKey(dict_reader* r, llarp_buffer_t* key)
  {
    if(!key)
      return true;

    LR_CommitRecord* self = static_cast< LR_CommitRecord* >(r->user);

    bool read = false;

    if(!BEncodeMaybeReadDictEntry("c", self->commkey, read, *key, r->buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("i", self->nextHop, read, *key, r->buffer))
      return false;
    if(BEncodeMaybeReadDictEntry("n", self->tunnelNonce, read, *key, r->buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("p", self->pathid, read, *key, r->buffer))
      return false;
    if(!BEncodeMaybeReadVersion("v", self->version, LLARP_PROTO_VERSION, read,
                                *key, r->buffer))
      return false;
    if(llarp_buffer_eq(*key, "w"))
    {
      // check for duplicate
      if(self->work)
        return false;

      self->work = new PoW;
      return self->work->BDecode(r->buffer);
    }
    return read;
  }

  bool
  LR_CommitRecord::BDecode(llarp_buffer_t* buf)
  {
    dict_reader r;
    r.user   = this;
    r.on_key = &OnKey;
    return bencode_read_dict(buf, &r);
  }

  struct LRCMFrameDecrypt
  {
    typedef AsyncFrameDecrypter< LRCMFrameDecrypt > Decrypter;
    Decrypter* decrypter;
    std::deque< EncryptedFrame > frames;
    PathContext* context;
    // decrypted record
    LR_CommitRecord record;
    // the actual hop
    TransitHop hop;

    LRCMFrameDecrypt(PathContext* ctx, Decrypter* dec,
                     const LR_CommitMessage* commit)
        : decrypter(dec), context(ctx)
    {
      for(const auto& f : commit->frames)
        frames.push_front(f);
      hop.info.downstream = commit->remote;
    }

    ~LRCMFrameDecrypt()
    {
      delete decrypter;
    }

    /// this must be done from logic thread
    static void
    SendLRCM(void* user)
    {
      LRCMFrameDecrypt* self = static_cast< LRCMFrameDecrypt* >(user);
      self->context->ForwardLRCM(self->hop.info.upstream, self->frames);
      delete self;
    }

    static void
    SendLRAM(void* user)
    {
    }

    static void
    HandleDecrypted(llarp_buffer_t* buf, LRCMFrameDecrypt* self)
    {
      auto& info = self->hop.info;
      if(!buf)
      {
        llarp::Error("LRCM decrypt failed from ", info.downstream);
        delete self;
        return;
      }
      llarp::Debug("decrypted LRCM from ", info.downstream);
      // successful decrypt
      if(!self->record.BDecode(buf))
      {
        llarp::Error("malformed frame inside LRCM from ", info.downstream);
        delete self;
        return;
      }

      info.pathID   = self->record.pathid;
      info.upstream = self->record.nextHop;
      if(self->context->HasTransitHop(info))
      {
        llarp::Error("duplicate transit hop ", info);
        delete self;
        return;
      }
      // generate path key as we are in a worker thread
      auto DH = self->context->Crypto()->dh_server;
      if(!DH(self->hop.pathKey, self->record.commkey,
             self->context->EncryptionSecretKey(), self->record.tunnelNonce))
      {
        llarp::Error("LRCM DH Failed ", info);
        delete self;
        return;
      }
      if(self->record.work
         && self->record.work->IsValid(self->context->Crypto()->shorthash,
                                       self->context->OurRouterID()))
      {
        llarp::Info("LRCM extended lifetime by ",
                    self->record.work->extendedLifetime, " seconds for ", info);
        self->hop.lifetime += 1000 * self->record.work->extendedLifetime;
      }

      // TODO: check if we really want to accept it
      self->hop.started = llarp_time_now_ms();
      llarp::Info("Accepted ", self->hop.info);
      self->context->PutTransitHop(self->hop);

      size_t sz = self->frames.front().size;
      // we pop the front element it was ours
      self->frames.pop_front();
      // put our response on the end
      self->frames.emplace_back(sz);
      auto& reply   = self->frames.back();
      auto replybuf = reply.Buffer();
      LR_AcceptRecord replyrecord;
      replyrecord.upstream   = info.upstream;
      replyrecord.downstream = info.downstream;
      replyrecord.pathid     = info.pathID;
      if(!replyrecord.BEncode(replybuf))
      {
        llarp::Error("failed to encode reply to LRCM, buffer too small?");
        delete self;
        return;
      }
      // randomize leftover data inside reply
      auto left = llarp_buffer_size_left(*replybuf);
      if(left)
        self->context->Crypto()->randbytes(replybuf->cur, left);

      // encrypt in place since we are in the worker thread
      if(!reply.EncryptInPlace(self->context->EncryptionSecretKey(),
                               self->record.commkey, self->context->Crypto()))
      {
        // failed to encrypt wtf?
        llarp::Error("Failed to encrypt reply to LRCM");
        delete self;
        return;
      }
      if(self->context->HopIsUs(info.upstream))
      {
        // we are the farthest hop
        llarp::Info("We are the farthest hop for ", info);
        // send a LRAM down the path
        llarp_logic_queue_job(self->context->Logic(), {self, &SendLRAM});
      }
      else
      {
        // forward upstream
        // we are still in the worker thread so post job to logic
        llarp_logic_queue_job(self->context->Logic(), {self, &SendLRCM});
      }
    }
  };

  bool
  LR_CommitMessage::AsyncDecrypt(PathContext* context) const
  {
    LRCMFrameDecrypt::Decrypter* decrypter = new LRCMFrameDecrypt::Decrypter(
        context->Crypto(), context->EncryptionSecretKey(),
        &LRCMFrameDecrypt::HandleDecrypted);
    // copy frames so we own them
    LRCMFrameDecrypt* frames = new LRCMFrameDecrypt(context, decrypter, this);

    // decrypt frames async
    decrypter->AsyncDecrypt(context->Worker(), &frames->frames.front(), frames);
    return true;
  }
}  // namespace llarp
