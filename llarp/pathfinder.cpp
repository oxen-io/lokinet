#include <deque>
#include <llarp/encrypted_frame.hpp>
#include <llarp/path.hpp>
#include "router.hpp"

namespace llarp
{
  PathContext::PathContext(llarp_router* router)
      : m_Router(router), m_AllowTransit(false)
  {
  }

  PathContext::~PathContext()
  {
  }

  void
  PathContext::AllowTransit()
  {
    m_AllowTransit = true;
  }

  struct LRCMFrameDecrypt
  {
    typedef AsyncFrameDecrypter< LRCMFrameDecrypt > Decrypter;
    Decrypter* decrypter;
    std::deque< EncryptedFrame > frames;
    std::deque< EncryptedAck > acks;
    EncryptedFrame lastFrame;
    PathContext* context;
    RouterID from;
    LR_CommitRecord record;
    TransitHopInfo info;
    TransitHop hop;

    LRCMFrameDecrypt(PathContext* ctx, Decrypter* dec,
                     const LR_CommitMessage* commit)
        : decrypter(dec)
        , lastFrame(commit->lasthopFrame)
        , context(ctx)
        , from(commit->remote)
    {
      for(const auto& f : commit->frames)
        frames.push_front(f);
      for(const auto& a : commit->acks)
        acks.push_front(a);
    }

    ~LRCMFrameDecrypt()
    {
      delete decrypter;
    }

    static void
    AcceptLRCM(void* user)
    {
      LRCMFrameDecrypt* self = static_cast< LRCMFrameDecrypt* >(user);
      llarp::Info("Accepted ", self->info);
      self->context->PutPendingRelayCommit(self->info.upstream, self->info.txID,
                                           self->info, self->hop);
      size_t sz = self->frames.front().size;
      // we pop the front element it was ours
      self->frames.pop_front();
      // put random on the end
      // TODO: should this be an encrypted frame?
      // TODO: should we change the size?
      self->frames.emplace_back(sz);
      self->frames.back().Randomize();
      self->context->ForwardLRCM(self->info.upstream, self->frames, self->acks,
                                 self->lastFrame);
      delete self;
    }

    static void
    HandleDecrypted(llarp_buffer_t* buf, LRCMFrameDecrypt* self)
    {
      if(!buf)
      {
        llarp::Error("LRCM decrypt failed from ", self->from);
        delete self;
        return;
      }
      llarp::Debug("decrypted LRCM from ", self->from);
      // successful decrypt
      if(!self->record.BDecode(buf))
      {
        llarp::Error("malformed frame inside LRCM from ", self->from);
        delete self;
        return;
      }
      self->info = TransitHopInfo(self->from, self->record);
      if(self->context->HasTransitHop(self->info))
      {
        llarp::Error("duplicate transit hop ", self->info);
        delete self;
        return;
      }
      // choose rx id
      // TODO: check for duplicates
      self->info.rxID.Randomize();

      // generate tx key as we are in a worker thread
      auto DH = self->context->Crypto()->dh_server;
      if(!DH(self->hop.txKey, self->record.commkey,
             self->context->EncryptionSecretKey(), self->record.tunnelNonce))
      {
        llarp::Error("LRCM DH Failed ", self->info);
        delete self;
        return;
      }
      if(self->context->HopIsUs(self->record.nextHop))
      {
        // we are the farthest hop
        llarp::Info("We are the farthest hop for ", self->info);
        // TODO: implement
        delete self;
      }
      else
      {
        // TODO: generate rx path

        // forward upstream
        // XXX: we are still in the worker thread so post job to logic
        llarp_logic_queue_job(self->context->Logic(), {self, &AcceptLRCM});
      }
    }
  };

  bool
  PathContext::HandleRelayCommit(const LR_CommitMessage* commit)
  {
    if(!m_AllowTransit)
    {
      llarp::Error("got LRCM from ", commit->remote,
                   " when not allowing transit traffic");
      return false;
    }
    if(commit->frames.size() <= 1)
    {
      llarp::Error("got LRCM with too few frames from ", commit->remote);
      return false;
    }
    LRCMFrameDecrypt::Decrypter* decrypter =
        new LRCMFrameDecrypt::Decrypter(&m_Router->crypto, m_Router->encryption,
                                        &LRCMFrameDecrypt::HandleDecrypted);
    // copy frames so we own them
    LRCMFrameDecrypt* frames = new LRCMFrameDecrypt(this, decrypter, commit);

    // decrypt frames async
    decrypter->AsyncDecrypt(m_Router->tp, &frames->frames.front(), frames);

    return true;
  }

  llarp_threadpool*
  PathContext::Worker()
  {
    return m_Router->tp;
  }

  llarp_crypto*
  PathContext::Crypto()
  {
    return &m_Router->crypto;
  }

  llarp_logic*
  PathContext::Logic()
  {
    return m_Router->logic;
  }

  byte_t*
  PathContext::EncryptionSecretKey()
  {
    return m_Router->encryption;
  }

  bool
  PathContext::HopIsUs(const PubKey& k) const
  {
    return memcmp(k, m_Router->pubkey(), PUBKEYSIZE) == 0;
  }

  bool
  PathContext::ForwardLRCM(const RouterID& nextHop,
                           std::deque< EncryptedFrame >& frames,
                           std::deque< EncryptedAck >& acks,
                           EncryptedFrame& lastHop)
  {
    LR_CommitMessage* msg = new LR_CommitMessage;
    while(frames.size())
    {
      msg->frames.push_back(frames.back());
      frames.pop_back();
    }
    while(acks.size())
    {
      msg->acks.push_back(acks.back());
      acks.pop_back();
    }
    msg->lasthopFrame = lastHop;
    return m_Router->SendToOrQueue(nextHop, {msg});
  }

  template < typename Map_t, typename Value_t >
  bool
  MapHas(Map_t& map, const Value_t& val)
  {
    std::unique_lock< std::mutex > lock(map.first);
    return map.second.find(val) != map.second.end();
  }

  template < typename Map_t, typename Key_t, typename Value_t >
  void
  MapPut(Map_t& map, const Key_t& k, const Value_t& v)
  {
    std::unique_lock< std::mutex > lock(map.first);
    map.second[k] = v;
  }

  bool
  PathContext::HasTransitHop(const TransitHopInfo& info)
  {
    return MapHas(m_TransitPaths, info);
  }

  void
  PathContext::PutPendingRelayCommit(const RouterID& upstream,
                                     const PathID_t& txid,
                                     const TransitHopInfo& info,
                                     const TransitHop& hop)
  {
    MapPut(m_WaitingForAcks, PendingPathKey(upstream, txid),
           std::make_pair(info, hop));
  }

  bool
  PathContext::HasPendingRelayCommit(const RouterID& upstream,
                                     const PathID_t& txid)
  {
    return MapHas(m_WaitingForAcks, PendingPathKey(upstream, txid));
  }

  TransitHopInfo::TransitHopInfo(const RouterID& down,
                                 const LR_CommitRecord& record)
      : txID(record.txid), upstream(record.nextHop), downstream(down)
  {
  }
}