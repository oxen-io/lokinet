#include <messages/relay_commit.hpp>

#include <messages/path_confirm.hpp>
#include <path/path.hpp>
#include <router/abstractrouter.hpp>
#include <util/bencode.hpp>
#include <util/buffer.hpp>
#include <util/logger.hpp>
#include <util/logic.hpp>
#include <nodedb.hpp>

#include <functional>

namespace llarp
{
  LR_CommitMessage::~LR_CommitMessage()
  {
  }

  bool
  LR_CommitMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if(key == "c")
    {
      return BEncodeReadArray(frames, buf);
    }
    bool read = false;
    if(!BEncodeMaybeReadVersion("v", version, LLARP_PROTO_VERSION, read, key,
                                buf))
      return false;

    return read;
  }

  void
  LR_CommitMessage::Clear()
  {
    frames[0].Clear();
    frames[1].Clear();
    frames[2].Clear();
    frames[3].Clear();
    frames[4].Clear();
    frames[5].Clear();
    frames[6].Clear();
    frames[7].Clear();
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
  LR_CommitMessage::HandleMessage(AbstractRouter* router) const
  {
    if(frames.size() != path::max_len)
    {
      llarp::LogError("LRCM invalid number of records, ", frames.size(),
                      "!=", path::max_len);
      return false;
    }
    if(!router->pathContext().AllowingTransit())
    {
      llarp::LogError("got LRCM when not permitting transit");
      return false;
    }
    return AsyncDecrypt(&router->pathContext());
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
    if(nextRC)
    {
      if(!BEncodeWriteDictEntry("u", *nextRC, buf))
        return false;
    }
    if(!bencode_write_version_entry(buf))
      return false;
    if(work && !BEncodeWriteDictEntry("w", *work, buf))
      return false;

    return bencode_end(buf);
  }

  bool
  LR_CommitRecord::OnKey(llarp_buffer_t* buffer, llarp_buffer_t* key)
  {
    if(!key)
      return true;

    bool read = false;

    if(!BEncodeMaybeReadDictEntry("c", commkey, read, *key, buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("i", nextHop, read, *key, buffer))
      return false;
    if(!BEncodeMaybeReadDictInt("l", lifetime, read, *key, buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("n", tunnelNonce, read, *key, buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("r", rxid, read, *key, buffer))
      return false;
    if(!BEncodeMaybeReadDictEntry("t", txid, read, *key, buffer))
      return false;
    if(*key == "u")
    {
      nextRC = std::make_unique< RouterContact >();
      return nextRC->BDecode(buffer);
    }
    if(!BEncodeMaybeReadVersion("v", version, LLARP_PROTO_VERSION, read, *key,
                                buffer))
      return false;
    if(*key == "w")
    {
      // check for duplicate
      if(work)
      {
        llarp::LogWarn("duplicate POW in LRCR");
        return false;
      }

      work = std::make_unique< PoW >();
      return bencode_decode_dict(*work, buffer);
    }
    return read;
  }

  bool
  LR_CommitRecord::BDecode(llarp_buffer_t* buf)
  {
    using namespace std::placeholders;
    return bencode_read_dict(std::bind(&LR_CommitRecord::OnKey, this, _1, _2),
                             buf);
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
    using Decrypter_ptr = std::unique_ptr< Decrypter >;
    Decrypter_ptr decrypter;
    std::array< EncryptedFrame, 8 > frames;
    Context* context;
    // decrypted record
    LR_CommitRecord record;
    // the actual hop
    std::shared_ptr< Hop > hop;

    LRCMFrameDecrypt(Context* ctx, Decrypter_ptr dec,
                     const LR_CommitMessage* commit)
        : decrypter(std::move(dec))
        , frames(commit->frames)
        , context(ctx)
        , hop(std::make_shared< Hop >())
    {
      hop->info.downstream = commit->session->GetPubKey();
    }

    ~LRCMFrameDecrypt()
    {
    }

    /// this is done from logic thread
    static void
    SendLRCM(std::shared_ptr< LRCMFrameDecrypt > self)
    {
      if(!self->context->Router()->ConnectionToRouterAllowed(
             self->hop->info.upstream))
      {
        // we are not allowed to forward it ... now what?
        llarp::LogError("path to ", self->hop->info.upstream,
                        "not allowed, dropping build request on the floor");
        self->hop = nullptr;
        return;
      }
      // persist sessions to upstream and downstream routers until the commit
      // ends
      self->context->Router()->PersistSessionUntil(
          self->hop->info.downstream, self->hop->ExpireTime() + 10000);
      self->context->Router()->PersistSessionUntil(
          self->hop->info.upstream, self->hop->ExpireTime() + 10000);
      // put hop
      self->context->PutTransitHop(self->hop);
      // if we have an rc for this hop...
      if(self->record.nextRC)
      {
        // ... and it matches the next hop ...
        if(self->record.nextHop == self->record.nextRC->pubkey)
        {
          // ... and it's valid
          const auto now = self->context->Router()->Now();
          if(self->record.nextRC->IsPublicRouter()
             && self->record.nextRC->Verify(self->context->crypto(), now))
          {
            llarp_nodedb* n        = self->context->Router()->nodedb();
            const RouterContact rc = *self->record.nextRC;
            // store it into netdb if we don't have it
            if(!n->Has(rc.pubkey))
            {
              std::shared_ptr< Logic > l = self->context->Router()->logic();
              n->InsertAsync(rc, l, [=]() {
                self->context->ForwardLRCM(self->hop->info.upstream,
                                           self->frames);
                self->hop = nullptr;
              });
              return;
            }
          }
        }
      }
      // forward to next hop
      self->context->ForwardLRCM(self->hop->info.upstream, self->frames);
      self->hop = nullptr;
    }

    // this is called from the logic thread
    static void
    SendPathConfirm(std::shared_ptr< LRCMFrameDecrypt > self)
    {
      // persist session to downstream until path expiration
      self->context->Router()->PersistSessionUntil(
          self->hop->info.downstream, self->hop->ExpireTime() + 10000);
      // put hop
      self->context->PutTransitHop(self->hop);
      // send path confirmation
      const llarp::routing::PathConfirmMessage confirm(self->hop->lifetime);
      if(!self->hop->SendRoutingMessage(confirm, self->context->Router()))
      {
        llarp::LogError("failed to send path confirmation for ",
                        self->hop->info);
      }
      self->hop = nullptr;
    }

    static void
    HandleDecrypted(llarp_buffer_t* buf,
                    std::shared_ptr< LRCMFrameDecrypt > self)
    {
      auto now   = self->context->Router()->Now();
      auto& info = self->hop->info;
      if(!buf)
      {
        llarp::LogError("LRCM decrypt failed from ", info.downstream);
        self->decrypter = nullptr;
        return;
      }
      buf->cur = buf->base + EncryptedFrameOverheadSize;
      llarp::LogDebug("decrypted LRCM from ", info.downstream);
      // successful decrypt
      if(!self->record.BDecode(buf))
      {
        llarp::LogError("malformed frame inside LRCM from ", info.downstream);
        self->decrypter = nullptr;
        return;
      }

      info.txID     = self->record.txid;
      info.rxID     = self->record.rxid;
      info.upstream = self->record.nextHop;
      if(self->context->HasTransitHop(info))
      {
        llarp::LogError("duplicate transit hop ", info);
        self->decrypter = nullptr;
        return;
      }
      // generate path key as we are in a worker thread
      auto crypto = self->context->crypto();
      if(!crypto->dh_server(self->hop->pathKey, self->record.commkey,
                            self->context->EncryptionSecretKey(),
                            self->record.tunnelNonce))
      {
        llarp::LogError("LRCM DH Failed ", info);
        self->decrypter = nullptr;
        return;
      }
      // generate hash of hop key for nonce mutation
      crypto->shorthash(self->hop->nonceXOR,
                        llarp_buffer_t(self->hop->pathKey));
      using namespace std::placeholders;
      if(self->record.work
         && self->record.work->IsValid(
             std::bind(&Crypto::shorthash, crypto, _1, _2), now))
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
      self->hop->started = now;

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
      frames[7] = EncryptedFrame(sz - EncryptedFrameOverheadSize);
      // random junk for now
      frames[7].Randomize();
      self->frames = std::move(frames);
      if(self->context->HopIsUs(info.upstream))
      {
        // we are the farthest hop
        llarp::LogDebug("We are the farthest hop for ", info);
        // send a LRAM down the path
        self->context->logic()->queue_func([=]() {
          SendPathConfirm(self);
          self->decrypter = nullptr;
        });
      }
      else
      {
        // forward upstream
        // we are still in the worker thread so post job to logic
        self->context->logic()->queue_func([=]() {
          SendLRCM(self);
          self->decrypter = nullptr;
        });
      }
    }
  };

  bool
  LR_CommitMessage::AsyncDecrypt(llarp::path::PathContext* context) const
  {
    auto decrypter = std::make_unique< LRCMFrameDecrypt::Decrypter >(
        context->crypto(), context->EncryptionSecretKey(),
        &LRCMFrameDecrypt::HandleDecrypted);
    // copy frames so we own them
    auto frameDecrypt = std::make_shared< LRCMFrameDecrypt >(
        context, std::move(decrypter), this);

    // decrypt frames async
    frameDecrypt->decrypter->AsyncDecrypt(
        context->Worker(), frameDecrypt->frames[0], frameDecrypt);
    return true;
  }
}  // namespace llarp
