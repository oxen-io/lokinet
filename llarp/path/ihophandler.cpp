#include <path/ihophandler.hpp>
#include <router/router.hpp>

namespace llarp
{
  namespace path
  {
    // handle data in upstream direction
    bool
    IHopHandler::HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                                AbstractRouter* r)
    {
      if(m_UpstreamPool == nullptr)
      {
        m_UpstreamPool = ObtainUpstreamBufferPool();
        if(m_UpstreamPool == nullptr)
          return false;
      }
      if(m_UpstreamPool->AllocBuffer([&](auto& buf) -> bool {
           if(buf.IsFull())
           {
             FlushUpstream(r);
             m_UpstreamPool->EnsureBufferSpace();
             return buf.Index == m_UpstreamPool->Current().Index;
           }
           return false;
         })
         == nullptr)
        return false;
      bool success = true;
      m_UpstreamPool->UseCurrentBuffer(
          [&X, &Y, &success](auto& bucket, auto idx) {
            if(bucket.IsFull())
              success = false;
            else
              bucket.GiveMessageAt(X, Y, idx);
          });
      return success;
    }

    // handle data in downstream direction
    bool
    IHopHandler::HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                                  AbstractRouter* r)
    {
      if(m_DownstreamPool == nullptr)
      {
        m_DownstreamPool = ObtainDownstreamBufferPool();
        if(m_DownstreamPool == nullptr)
          return false;
      }
      if(m_DownstreamPool->AllocBuffer([&](auto& buf) -> bool {
           if(buf.IsFull())
           {
             FlushDownstream(r);
             m_DownstreamPool->EnsureBufferSpace();
             return buf.Index == m_DownstreamPool->Current().Index;
           }
           return false;
         })
         == nullptr)
        return false;
      bool success = true;
      m_DownstreamPool->UseCurrentBuffer(
          [&X, &Y, &success](auto& bucket, auto idx) {
            if(bucket.IsFull())
              success = false;
            else
              bucket.GiveMessageAt(X, Y, idx);
          });
      return success;
    }

    void
    IHopHandler::CollectUpstream(AbstractRouter* r, UpstreamTraffic_ptr batch)
    {
      if(m_UpstreamEgress.empty())
      {
        r->logic()->queue_func([self = GetSelf(), r] {
          while(not self->m_UpstreamEgress.empty())
          {
            UpstreamTraffic_ptr t = self->m_UpstreamEgress.top();
            self->HandleAllUpstream(t, r);
            self->FreeUpstream(t);
            self->m_UpstreamEgress.pop();
          }
          self->AfterCollectUpstream(r);
        });
      }
      m_UpstreamEgress.emplace(batch);
    }

    void
    IHopHandler::CollectDownstream(AbstractRouter* r,
                                   DownstreamTraffic_ptr batch)
    {
      if(m_DownstreamEgress.empty())
      {
        r->logic()->queue_func([self = GetSelf(), r] {
          while(not self->m_DownstreamEgress.empty())
          {
            DownstreamTraffic_ptr t = self->m_DownstreamEgress.top();
            self->HandleAllDownstream(t, r);
            self->FreeDownstream(t);
            self->m_DownstreamEgress.pop();
          }
          self->AfterCollectDownstream(r);
        });
      }
      m_DownstreamEgress.emplace(batch);
    }

    void
    IHopHandler::FreeUpstream(UpstreamTraffic_ptr t)
    {
      m_UpstreamPool->FreeBuffer(t);
    }

    void
    IHopHandler::FreeDownstream(DownstreamTraffic_ptr t)
    {
      m_DownstreamPool->FreeBuffer(t);
    }

    void
    IHopHandler::FlushUpstream(AbstractRouter* r)
    {
      if(m_UpstreamPool == nullptr)
        return;
      auto pool = m_UpstreamPool->PopNextBuffer();
      if(pool)
      {
        if(pool->empty())
        {
          m_UpstreamPool->FreeBuffer(pool);
        }
        else
        {
          pool->seqno = m_UpstreamSequence++;
          r->pathContext().QueuePathWork(
              std::bind(&IHopHandler::UpstreamWork, GetSelf(), pool, r));
        }
      }
    }

    void
    IHopHandler::FlushDownstream(AbstractRouter* r)
    {
      if(m_DownstreamPool == nullptr)
        return;
      auto pool = m_DownstreamPool->PopNextBuffer();
      if(pool)
      {
        if(pool->empty())
        {
          m_DownstreamPool->FreeBuffer(pool);
        }
        else
        {
          pool->seqno = m_DownstreamSequence++;
          r->pathContext().QueuePathWork(
              std::bind(&IHopHandler::DownstreamWork, GetSelf(), pool, r));
        }
      }
    }

  }  // namespace path
}  // namespace llarp