#include <llarp/bencode.hpp>
#include <llarp/messages/path_confirm.hpp>
#include <llarp/messages/relay_commit.hpp>
#include "buffer.hpp"
#include "logger.hpp"
#include "router.hpp"

namespace llarp
{
  LR_CommitMessage::~LR_CommitMessage()
  {
  }

  bool
  LR_CommitMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
  {
    if(llarp_buffer_eq(key, "c"))
    {
      return BEncodeReadArray(frames, buf);
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
    if(!BEncodeWriteDictArray("c", frames, buf))
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
      llarp::LogError("LRCM invalid number of records, ", frames.size(),
                      "!=", MAXHOPS);
      return false;
    }
    if(!router->paths.AllowingTransit())
    {
      llarp::LogError("got LRCM when not permitting transit");
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
    if(lifetime > 10 && lifetime < 600)
    {
      if(!BEncodeWriteDictInt("i", lifetime, buf))
        return false;
    }
    if(!BEncodeWriteDictEntry("n", tunnelNonce, buf))
      return false;
    if(!BEncodeWriteDictEntry("r", rxid, buf))
      return false;
    if(!BEncodeWriteDictEntry("t", txid, buf))
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
    if(!BEncodeMaybeReadDictInt("l", self->lifetime, read, *key, r->buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("n", self->tunnelNonce, read, *key,
                                  r->buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("r", self->rxid, read, *key, r->buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("t", self->txid, read, *key, r->buffer))
      return false;
    if(!BEncodeMaybeReadVersion("v", self->version, LLARP_PROTO_VERSION, read,
                                *key, r->buffer))
      return false;
    if(llarp_buffer_eq(*key, "w"))
    {
      // check for duplicate
      if(self->work)
      {
        llarp::LogWarn("duplicate POW in LRCR");
        return false;
      }

      self->work = new PoW();
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

  bool
  LR_CommitRecord::operator==(const LR_CommitRecord& other) const
  {
    if(work && other.work)
    {
      if(*work != *other.work)
        return false;
    }
    return nextHop == other.nextHop && commkey == other.commkey
        && txid == other.txid && rxid == other.rxid;
  }

  struct LRCMFrameDecrypt
  {
    typedef llarp::path::PathContext Context;
    typedef llarp::path::TransitHop Hop;
    typedef AsyncFrameDecrypter< LRCMFrameDecrypt > Decrypter;
    Decrypter* decrypter;
    std::array< EncryptedFrame, 8 > frames;
    Context* context;
    // decrypted record
    LR_CommitRecord record;
    // the actual hop
    std::shared_ptr< Hop > hop;

    LRCMFrameDecrypt(Context* ctx, Decrypter* dec,
                     const LR_CommitMessage* commit)
        : decrypter(dec), frames(commit->frames), context(ctx), hop(new Hop())
    {
      hop->info.downstream = commit->session->GetPubKey();
    }

    ~LRCMFrameDecrypt()
    {
      if(hop) delete hop;
         
      delete decrypter;
    }

    /// this is done from logic thread
    static void
    SendLRCM(void* user)
    {
      LRCMFrameDecrypt* self = static_cast< LRCMFrameDecrypt* >(user);
      // persist sessions to upstream and downstream routers until the commit
      // ends
      self->context->Router()->PersistSessionUntil(self->hop->info.downstream,
                                                   self->hop->ExpireTime());
      self->context->Router()->PersistSessionUntil(self->hop->info.upstream,
                                                   self->hop->ExpireTime());
      // put hop
      self->context->PutTransitHop(self->hop);
      // forward to next hop
      self->context->ForwardLRCM(self->hop->info.upstream, self->frames);
      self->hop = nullptr;
      delete self;
    }

    // this is called from the logic thread
    static void
    SendPathConfirm(void* user)
    {
      LRCMFrameDecrypt* self = static_cast< LRCMFrameDecrypt* >(user);
      // persist session to downstream until path expiration
      self->context->Router()->PersistSessionUntil(self->hop->info.downstream,
                                                   self->hop->ExpireTime());
      // put hop
      self->context->PutTransitHop(self->hop);
      // send path confirmation
      llarp::routing::PathConfirmMessage confirm(self->hop->lifetime);
      if(!self->hop->SendRoutingMessage(&confirm, self->context->Router()))
      {
        llarp::LogError("failed to send path confirmation for ",
                        self->hop->info);
      }
      self->hop = nullptr;
      delete self;
    }

    static void
    HandleDecrypted(llarp_buffer_t* buf, LRCMFrameDecrypt* self)
    {
      auto& info = self->hop->info;
      if(!buf)
      {
        llarp::LogError("LRCM decrypt failed from ", info.downstream);
        delete self;
        return;
      }
      buf->cur = buf->base + EncryptedFrame::OverheadSize;
      llarp::LogDebug("decrypted LRCM from ", info.downstream);
      // successful decrypt
      if(!self->record.BDecode(buf))
      {
        llarp::LogError("malformed frame inside LRCM from ", info.downstream);
        delete self;
        return;
      }

      info.txID     = self->record.txid;
      info.rxID     = self->record.rxid;
      info.upstream = self->record.nextHop;
      if(self->context->HasTransitHop(info))
      {
        llarp::LogError("duplicate transit hop ", info);
        delete self;
        return;
      }
      // generate path key as we are in a worker thread
      auto DH = self->context->Crypto()->dh_server;
      if(!DH(self->hop->pathKey, self->record.commkey,
             self->context->EncryptionSecretKey(), self->record.tunnelNonce))
      {
        llarp::LogError("LRCM DH Failed ", info);
        delete self;
        return;
      }
      // generate hash of hop key for nonce mutation
      self->context->Crypto()->shorthash(self->hop->nonceXOR,
                                         llarp::Buffer(self->hop->pathKey));
      if(self->record.work
         && self->record.work->IsValid(self->context->Crypto()->shorthash))
      {
        llarp::LogDebug("LRCM extended lifetime by ",
                        self->record.work->extendedLifetime, " seconds for ",
                        info);
        self->hop->lifetime += 1000 * self->record.work->extendedLifetime;
      }
      else if(self->record.lifetime < 600 && self->record.lifetime > 10)
      {
        self->hop->lifetime = self->record.lifetime;
        llarp::LogDebug("LRCM short lifespan set to ", self->hop->lifetime,
                        " seconds for ", info);
      }

      // TODO: check if we really want to accept it
      self->hop->started = llarp_time_now_ms();

      size_t sz = self->frames[0].size();
      // shift
      std::array< EncryptedFrame, 8 > frames;
      frames[0] = self->frames[1];
      frames[1] = self->frames[2];
      frames[2] = self->frames[3];
      frames[3] = self->frames[4];
      frames[4] = self->frames[5];
      frames[5] = self->frames[6];
      frames[6] = self->frames[7];
      // put our response on the end
      frames[7] = EncryptedFrame(sz - EncryptedFrame::OverheadSize);
      // random junk for now
      frames[7].Randomize();
      self->frames = std::move(frames);
      if(self->context->HopIsUs(info.upstream))
      {
        // we are the farthest hop
        llarp::LogDebug("We are the farthest hop for ", info);
        // send a LRAM down the path
        llarp_logic_queue_job(self->context->Logic(), {self, &SendPathConfirm});
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
  LR_CommitMessage::AsyncDecrypt(llarp::path::PathContext* context) const
  {
    LRCMFrameDecrypt::Decrypter* decrypter = new LRCMFrameDecrypt::Decrypter(
        context->Crypto(), context->EncryptionSecretKey(),
        &LRCMFrameDecrypt::HandleDecrypted);
    // copy frames so we own them
    LRCMFrameDecrypt* frames = new LRCMFrameDecrypt(context, decrypter, this);

    // decrypt frames async
    decrypter->AsyncDecrypt(context->Worker(), &frames->frames[0], frames);
    return true;
  }
}  // namespace llarp
