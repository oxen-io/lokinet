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
    std::vector< EncryptedFrame > leftovers;
    EncryptedFrame ourFrame;
    PathContext* context;
    RouterID from;
    LR_CommitRecord record;

    LRCMFrameDecrypt(PathContext* ctx, Decrypter* dec,
                     const LR_CommitMessage* commit)
        : decrypter(dec), context(ctx), from(commit->remote)
    {
      auto sz    = commit->frames.size();
      size_t idx = 0;
      while(idx < sz)
      {
        if(sz == 0)
          ourFrame = commit->frames[idx];
        else
          leftovers.push_back(commit->frames[idx]);
        ++idx;
      }
    }

    ~LRCMFrameDecrypt()
    {
      delete decrypter;
    }

    static void
    HandleDecrypted(llarp_buffer_t* buf, LRCMFrameDecrypt* self)
    {
      if(buf)
      {
        llarp::Debug("decrypted LRCM from ", self->from);
        // successful decrypt
        if(self->record.BDecode(buf))
        {
          TransitHopInfo info(self->from, self->record);
          if(self->context->HasTransitHop(info))
          {
            // duplicate hop
            llarp::Warn("duplicate transit hop ", info);
          }
          else
          {
            // accepted
            return;
          }
        }
        else
          llarp::Error("malformed LR Commit Record from ", self->from);
      }
      else
        llarp::Debug("malformed frame inside LRCM from ", self->from);
      delete self;
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
    decrypter->AsyncDecrypt(m_Router->tp, &frames->ourFrame, frames);

    return true;
  }

  bool
  PathContext::HasTransitHop(const TransitHopInfo& info)
  {
    std::unique_lock< std::mutex > lock(m_TransitPathsMutex);
    return m_TransitPaths.find(info) != m_TransitPaths.end();
  }

  TransitHopInfo::TransitHopInfo(const RouterID& down,
                                 const LR_CommitRecord& record)
      : txID(record.txid), upstream(record.nextHop), downstream(down)
  {
  }
}