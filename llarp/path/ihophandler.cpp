#include <path/ihophandler.hpp>
#include <router/router.hpp>

namespace llarp
{
  namespace path
  {

    // handle data in upstream direction
    bool
    IHopHandler::HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                                AbstractRouter*)
    {
      if(m_UpstreamIngest == nullptr)
      {
        m_UpstreamIngest        = NewUpstream();
        if(m_UpstreamIngest == nullptr)
          return false;
      }
      if(m_UpstreamPool == nullptr)
        return false;
      m_UpstreamPool->UseCurrentBuffer([&X, &Y](auto & bucket, auto idx)
      {
        bucket.GiveMessageAt(X, Y, idx);
      });
      return true;
    }

    // handle data in downstream direction
    bool
    IHopHandler::HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                                  AbstractRouter*)
    {
      if(m_DownstreamIngest == nullptr)
      {
        m_DownstreamIngest = NewDownstream();
        if(m_DownstreamIngest == nullptr)
          return false;
      }
      if(m_DownstreamPool == nullptr)
        return false;
      m_DownstreamPool->UseCurrentBuffer([&X, &Y](auto & bucket, auto idx) {
        bucket.GiveMessageAt(X, Y, idx);
      });
      return true;
    }

    void
    IHopHandler::CollectUpstream(AbstractRouter* r,
                                  UpstreamTraffic_ptr batch)
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


    UpstreamTraffic_ptr
    IHopHandler::NewUpstream()
    {
      if(m_UpstreamPool == nullptr)
      {
        m_UpstreamPool = ObtainUpstreamBufferPool();
        if(m_UpstreamPool == nullptr)
          return nullptr;
      }
      auto t = m_UpstreamPool->AllocBuffer([&](auto & buf) -> bool {
        if(buf.IsFull())
        {
          m_UpstreamPool->EnsureBufferSpace();
          return m_UpstreamPool->Current().Index == buf.Index;
        }
        return false;
      });
      if(t)
      {
        t->seqno = m_UpstreamSequence++;
      }
      return t;
    }

    void 
    IHopHandler::FreeUpstream(UpstreamTraffic_ptr t)
    {
       if(m_UpstreamPool)
        m_UpstreamPool->FreeBuffer(t);
    }

    DownstreamTraffic_ptr
    IHopHandler::NewDownstream()
    {
      if(m_DownstreamPool == nullptr)
      {
        m_DownstreamPool = ObtainDownstreamBufferPool();
        if(m_DownstreamPool == nullptr)
          return nullptr;
      }
      auto t = m_DownstreamPool->AllocBuffer([&](auto & buf) -> bool {
        if(buf.IsFull())
        {
          m_DownstreamPool->EnsureBufferSpace();
          return m_DownstreamPool->Current().Index == buf.Index;
        }
        return false;
      });
      if(t)
      {
        t->seqno = m_DownstreamSequence++;
      }
      return t;
    }

    void 
    IHopHandler::FreeDownstream(DownstreamTraffic_ptr t)
    {
      if(m_DownstreamPool)
        m_DownstreamPool->FreeBuffer(t);
    }

    void
    IHopHandler::FlushUpstream(AbstractRouter* r)
    {
      if(m_UpstreamIngest && not m_UpstreamIngest->empty())
        r->pathContext().QueuePathWork(std::bind(&IHopHandler::UpstreamWork, GetSelf(),
                                          m_UpstreamIngest, r));

      m_UpstreamIngest = nullptr;
    }

    void
    IHopHandler::FlushDownstream(AbstractRouter* r)
    {
      if(m_DownstreamIngest && not m_DownstreamIngest->empty())
        r->pathContext().QueuePathWork(std::bind(&IHopHandler::DownstreamWork,
                                          GetSelf(),
                                          m_DownstreamIngest, r));
      m_DownstreamIngest = nullptr;
    }

  }  // namespace path
}  // namespace llarp