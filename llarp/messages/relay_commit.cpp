#include "relay_commit.hpp"
#include "relay_status.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/path/transit_hop.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/router/i_outbound_message_handler.hpp>
#include <llarp/routing/path_confirm_message.hpp>
#include <llarp/util/bencode.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/meta/memfn.hpp>
#include <llarp/tooling/path_event.hpp>

#include <functional>
#include <optional>

namespace llarp
{
  bool
  LR_CommitMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if (key.startswith("c"))
    {
      /// so we dont put it into the shitty queue
      pathid.Fill('c');
      return BEncodeReadArray(frames, buf);
    }
    bool read = false;
    if (!BEncodeMaybeVerifyVersion("v", version, llarp::constants::proto_version, read, key, buf))
      return false;

    return read;
  }

  void
  LR_CommitMessage::Clear()
  {
    std::for_each(frames.begin(), frames.end(), [](auto& f) { f.Clear(); });
    version = 0;
  }

  bool
  LR_CommitMessage::BEncode(llarp_buffer_t* buf) const
  {
    if (!bencode_start_dict(buf))
      return false;
    // msg type
    if (!BEncodeWriteDictMsgType(buf, "a", "c"))
      return false;
    // frames
    if (!BEncodeWriteDictArray("c", frames, buf))
      return false;
    // version
    if (!bencode_write_uint64_entry(buf, "v", 1, llarp::constants::proto_version))
      return false;

    return bencode_end(buf);
  }

  bool
  LR_CommitMessage::HandleMessage(AbstractRouter* router) const
  {
    if (frames.size() != path::max_len)
    {
      llarp::LogError("LRCM invalid number of records, ", frames.size(), "!=", path::max_len);
      return false;
    }
    if (!router->pathContext().AllowingTransit())
    {
      llarp::LogError("got LRCM when not permitting transit");
      return false;
    }
    return AsyncDecrypt(&router->pathContext());
  }

  bool
  LR_CommitRecord::BEncode(llarp_buffer_t* buf) const
  {
    if (!bencode_start_dict(buf))
      return false;

    if (!BEncodeWriteDictEntry("c", commkey, buf))
      return false;
    if (!BEncodeWriteDictEntry("i", nextHop, buf))
      return false;
    if (lifetime > 10s && lifetime < path::default_lifetime)
    {
      if (!BEncodeWriteDictInt("i", lifetime.count(), buf))
        return false;
    }
    if (!BEncodeWriteDictEntry("n", tunnelNonce, buf))
      return false;
    if (!BEncodeWriteDictEntry("r", rxid, buf))
      return false;
    if (!BEncodeWriteDictEntry("t", txid, buf))
      return false;
    if (nextRC)
    {
      if (!BEncodeWriteDictEntry("u", *nextRC, buf))
        return false;
    }

    if (not bencode_write_uint64_entry(buf, "v", 1, llarp::constants::proto_version))
      return false;
    if (work and not BEncodeWriteDictEntry("w", *work, buf))
      return false;

    return bencode_end(buf);
  }

  bool
  LR_CommitRecord::OnKey(llarp_buffer_t* buffer, llarp_buffer_t* key)
  {
    if (!key)
      return true;

    bool read = false;

    if (!BEncodeMaybeReadDictEntry("c", commkey, read, *key, buffer))
      return false;
    if (!BEncodeMaybeReadDictEntry("i", nextHop, read, *key, buffer))
      return false;
    if (!BEncodeMaybeReadDictInt("l", lifetime, read, *key, buffer))
      return false;
    if (!BEncodeMaybeReadDictEntry("n", tunnelNonce, read, *key, buffer))
      return false;
    if (!BEncodeMaybeReadDictEntry("r", rxid, read, *key, buffer))
      return false;
    if (!BEncodeMaybeReadDictEntry("t", txid, read, *key, buffer))
      return false;
    if (key->startswith("u"))
    {
      nextRC = std::make_unique<RouterContact>();
      return nextRC->BDecode(buffer);
    }
    if (!BEncodeMaybeVerifyVersion(
            "v", version, llarp::constants::proto_version, read, *key, buffer))
      return false;
    if (key->startswith("w"))
    {
      // check for duplicate
      if (work)
      {
        llarp::LogWarn("duplicate POW in LRCR");
        return false;
      }

      work = std::make_unique<PoW>();
      return bencode_decode_dict(*work, buffer);
    }
    return read;
  }

  bool
  LR_CommitRecord::BDecode(llarp_buffer_t* buf)
  {
    return bencode_read_dict(util::memFn(&LR_CommitRecord::OnKey, this), buf);
  }

  bool
  LR_CommitRecord::operator==(const LR_CommitRecord& other) const
  {
    if (work && other.work)
    {
      if (*work != *other.work)
        return false;
    }
    return nextHop == other.nextHop && commkey == other.commkey && txid == other.txid
        && rxid == other.rxid;
  }

  struct LRCMFrameDecrypt
  {
    using Context = llarp::path::PathContext;
    using Hop = llarp::path::TransitHop;
    using Decrypter = AsyncFrameDecrypter<LRCMFrameDecrypt>;
    using Decrypter_ptr = std::unique_ptr<Decrypter>;
    Decrypter_ptr decrypter;
    std::array<EncryptedFrame, 8> frames;
    Context* context;
    // decrypted record
    LR_CommitRecord record;
    // the actual hop
    std::shared_ptr<Hop> hop;

    const std::optional<IpAddress> fromAddr;

    LRCMFrameDecrypt(Context* ctx, Decrypter_ptr dec, const LR_CommitMessage* commit)
        : decrypter(std::move(dec))
        , frames(commit->frames)
        , context(ctx)
        , hop(std::make_shared<Hop>())
        , fromAddr(
              commit->session->GetRemoteRC().IsPublicRouter()
                  ? std::optional<IpAddress>{}
                  : commit->session->GetRemoteEndpoint())
    {
      hop->info.downstream = commit->session->GetPubKey();
    }

    ~LRCMFrameDecrypt() = default;

    static void
    OnForwardLRCMResult(
        AbstractRouter* router,
        std::shared_ptr<path::TransitHop> path,
        const PathID_t pathid,
        const RouterID nextHop,
        const SharedSecret pathKey,
        SendStatus sendStatus)
    {
      uint64_t status = LR_StatusRecord::FAIL_DEST_INVALID;

      switch (sendStatus)
      {
        case SendStatus::Success:
          // do nothing, will forward success message later
          return;
        case SendStatus::Timeout:
          status = LR_StatusRecord::FAIL_TIMEOUT;
          break;
        case SendStatus::NoLink:
          status = LR_StatusRecord::FAIL_CANNOT_CONNECT;
          break;
        case SendStatus::InvalidRouter:
          status = LR_StatusRecord::FAIL_DEST_INVALID;
          break;
        case SendStatus::RouterNotFound:
          status = LR_StatusRecord::FAIL_DEST_UNKNOWN;
          break;
        case SendStatus::Congestion:
          status = LR_StatusRecord::FAIL_CONGESTION;
          break;
        default:
          LogError("llarp::SendStatus value not in enum class");
          std::abort();
          break;
      }
      router->QueueWork([router, path, pathid, nextHop, pathKey, status] {
        LR_StatusMessage::CreateAndSend(router, path, pathid, nextHop, pathKey, status);
      });
    }

    /// this is done from logic thread
    static void
    SendLRCM(std::shared_ptr<LRCMFrameDecrypt> self)
    {
      if (self->context->HasTransitHop(self->hop->info))
      {
        llarp::LogError("duplicate transit hop ", self->hop->info);
        LR_StatusMessage::CreateAndSend(
            self->context->Router(),
            self->hop,
            self->hop->info.rxID,
            self->hop->info.downstream,
            self->hop->pathKey,
            LR_StatusRecord::FAIL_DUPLICATE_HOP);
        self->hop = nullptr;
        return;
      }

      if (self->fromAddr)
      {
        // only do ip limiting from non service nodes
#ifndef LOKINET_HIVE
        if (self->context->CheckPathLimitHitByIP(*self->fromAddr))
        {
          // we hit a limit so tell it to slow tf down
          llarp::LogError("client path build hit limit ", *self->fromAddr);
          OnForwardLRCMResult(
              self->context->Router(),
              self->hop,
              self->hop->info.rxID,
              self->hop->info.downstream,
              self->hop->pathKey,
              SendStatus::Congestion);
          self->hop = nullptr;
          return;
        }
#endif
      }

      if (not self->context->Router()->PathToRouterAllowed(self->hop->info.upstream))
      {
        // we are not allowed to forward it ... now what?
        llarp::LogError(
            "path to ",
            self->hop->info.upstream,
            "not allowed, dropping build request on the floor");
        OnForwardLRCMResult(
            self->context->Router(),
            self->hop,
            self->hop->info.rxID,
            self->hop->info.downstream,
            self->hop->pathKey,
            SendStatus::InvalidRouter);
        self->hop = nullptr;
        return;
      }
      // persist sessions to upstream and downstream routers until the commit
      // ends
      self->context->Router()->PersistSessionUntil(
          self->hop->info.downstream, self->hop->ExpireTime() + 10s);
      self->context->Router()->PersistSessionUntil(
          self->hop->info.upstream, self->hop->ExpireTime() + 10s);
      // put hop
      self->context->PutTransitHop(self->hop);
      // forward to next hop
      using std::placeholders::_1;
      auto func = [self](auto status) {
        OnForwardLRCMResult(
            self->context->Router(),
            self->hop,
            self->hop->info.rxID,
            self->hop->info.downstream,
            self->hop->pathKey,
            status);
        self->hop = nullptr;
      };
      self->context->ForwardLRCM(self->hop->info.upstream, self->frames, func);
      // trigger idempotent pump to ensure that the build messages propagate
      self->context->Router()->TriggerPump();
    }

    // this is called from the logic thread
    static void
    SendPathConfirm(std::shared_ptr<LRCMFrameDecrypt> self)
    {
      // send path confirmation
      // TODO: other status flags?
      uint64_t status = LR_StatusRecord::SUCCESS;
      if (self->context->HasTransitHop(self->hop->info))
      {
        status = LR_StatusRecord::FAIL_DUPLICATE_HOP;
      }
      else
      {
        // persist session to downstream until path expiration
        self->context->Router()->PersistSessionUntil(
            self->hop->info.downstream, self->hop->ExpireTime() + 10s);
        // put hop
        self->context->PutTransitHop(self->hop);
      }

      if (!LR_StatusMessage::CreateAndSend(
              self->context->Router(),
              self->hop,
              self->hop->info.rxID,
              self->hop->info.downstream,
              self->hop->pathKey,
              status))
      {
        llarp::LogError("failed to send path confirmation for ", self->hop->info);
      }
      self->hop = nullptr;
    }

    // TODO: If decryption has succeeded here but we otherwise don't
    //       want to or can't accept the path build request, send
    //       a status message saying as much.
    static void
    HandleDecrypted(llarp_buffer_t* buf, std::shared_ptr<LRCMFrameDecrypt> self)
    {
      auto now = self->context->Router()->Now();
      auto& info = self->hop->info;
      if (!buf)
      {
        llarp::LogError("LRCM decrypt failed from ", info.downstream);
        self->decrypter = nullptr;
        return;
      }
      buf->cur = buf->base + EncryptedFrameOverheadSize;
      llarp::LogDebug("decrypted LRCM from ", info.downstream);
      // successful decrypt
      if (!self->record.BDecode(buf))
      {
        llarp::LogError("malformed frame inside LRCM from ", info.downstream);
        self->decrypter = nullptr;
        return;
      }

      info.txID = self->record.txid;
      info.rxID = self->record.rxid;

      if (info.txID.IsZero() || info.rxID.IsZero())
      {
        llarp::LogError("LRCM refusing zero pathid");
        self->decrypter = nullptr;
        return;
      }

      info.upstream = self->record.nextHop;

      // generate path key as we are in a worker thread
      auto crypto = CryptoManager::instance();
      if (!crypto->dh_server(
              self->hop->pathKey,
              self->record.commkey,
              self->context->EncryptionSecretKey(),
              self->record.tunnelNonce))
      {
        llarp::LogError("LRCM DH Failed ", info);
        self->decrypter = nullptr;
        return;
      }
      // generate hash of hop key for nonce mutation
      crypto->shorthash(self->hop->nonceXOR, llarp_buffer_t(self->hop->pathKey));
      if (self->record.work && self->record.work->IsValid(now))
      {
        llarp::LogDebug(
            "LRCM extended lifetime by ", self->record.work->extendedLifetime, " for ", info);
        self->hop->lifetime += self->record.work->extendedLifetime;
      }
      else if (self->record.lifetime < path::default_lifetime && self->record.lifetime > 10s)
      {
        self->hop->lifetime = self->record.lifetime;
        llarp::LogDebug("LRCM short lifespan set to ", self->hop->lifetime, " for ", info);
      }

      // TODO: check if we really want to accept it
      self->hop->started = now;

      self->context->Router()->NotifyRouterEvent<tooling::PathRequestReceivedEvent>(
          self->context->Router()->pubkey(), self->hop);

      size_t sz = self->frames[0].size();
      // shift
      std::array<EncryptedFrame, 8> frames;
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
      if (self->context->HopIsUs(info.upstream))
      {
        // we are the farthest hop
        llarp::LogDebug("We are the farthest hop for ", info);
        // send a LRSM down the path
        self->context->loop()->call([self] {
          SendPathConfirm(self);
          self->decrypter = nullptr;
        });
      }
      else
      {
        // forward upstream
        // we are still in the worker thread so post job to logic
        self->context->loop()->call([self] {
          SendLRCM(self);
          self->decrypter = nullptr;
        });
      }
      // trigger idempotent pump to ensure that the build messages propagate
      self->context->Router()->TriggerPump();
    }
  };

  bool
  LR_CommitMessage::AsyncDecrypt(llarp::path::PathContext* context) const
  {
    auto decrypter = std::make_unique<LRCMFrameDecrypt::Decrypter>(
        context->EncryptionSecretKey(), &LRCMFrameDecrypt::HandleDecrypted);
    // copy frames so we own them
    auto frameDecrypt = std::make_shared<LRCMFrameDecrypt>(context, std::move(decrypter), this);

    // decrypt frames async
    frameDecrypt->decrypter->AsyncDecrypt(
        frameDecrypt->frames[0], frameDecrypt, [r = context->Router()](auto func) {
          r->QueueWork(std::move(func));
        });
    return true;
  }
}  // namespace llarp
