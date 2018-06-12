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
      TransitHopInfo info(self->from, self->record);
      if(self->context->HasTransitHop(info))
      {
        llarp::Error("duplicate transit hop ", info);
        delete self;
        return;
      }
      TransitHop hop;
      // choose rx id
      // TODO: check for duplicates
      info.rxID.Randomize();

      // generate tx key as we are in a worker thread
      auto DH = self->context->Crypto()->dh_server;
      if(!DH(hop.txKey, self->record.commkey,
             self->context->EncryptionSecretKey(), self->record.tunnelNonce))
      {
        llarp::Error("LRCM DH Failed ", info);
        delete self;
        return;
      }
      if(self->context->HopIsUs(self->record.nextHop))
      {
        // we are the farthest hop
        llarp::Info("We are the farthest hop for ", info);
      }
      else
      {
        llarp::Info("Accepted ", info);
        self->context->PutPendingRelayCommit(info.upstream, info.txID, info,
                                             hop);
        size_t sz = self->frames.front().size;
        // we pop the front element it was ours
        self->frames.pop_front();
        // put random on the end
        // TODO: should this be an encrypted frame?
        self->frames.emplace_back(sz);
        self->frames.back().Randomize();
        // forward upstream
        self->context->ForwardLRCM(info.upstream, self->frames, self->acks,
                                   self->lastFrame);
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
      msg->frames.push_back(frames.front());
      frames.pop_front();
    }
    while(acks.size())
    {
      msg->acks.push_back(acks.front());
      acks.pop_front();
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