#include <path/ihophandler.hpp>
#include <path/path_context.hpp>
#include <router/router.hpp>

namespace llarp
{
  namespace path
  {
    IHopHandler::IHopHandler(PathContext* ctx) : m_Context(ctx)
    {
    }

    // handle data in upstream direction
    bool
    IHopHandler::HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                                AbstractRouter* r)
    {
      static const auto nop = [](auto&) -> bool { return false; };
      if(m_UpstreamIngest == nullptr)
        m_UpstreamIngest = m_Context->AllocBuffer(nop);
      // can't allocate more
      if(m_UpstreamIngest == nullptr)
        return false;
      // batch is full so flush it
      if(m_UpstreamIngest->IsFull())
      {
        // flush
        FlushUpstream(r);
        // get next batch
        m_UpstreamIngest = m_Context->AllocBuffer(nop);
        if(m_UpstreamIngest == nullptr)
        {
          // can't allocate more so bail
          return false;
        }
      }
      m_UpstreamIngest->QueueTraffic(X, Y);
      return true;
    }

    // handle data in downstream direction
    bool
    IHopHandler::HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                                  AbstractRouter* r)
    {
      static const auto nop = [](auto&) -> bool { return false; };
      if(m_DownstreamIngest == nullptr)
        m_DownstreamIngest = m_Context->AllocBuffer(nop);
      // can't allocate more
      if(m_DownstreamIngest == nullptr)
        return false;
      // batch is full so flush it
      if(m_DownstreamIngest->IsFull())
      {
        // flush
        FlushDownstream(r);
        // get next batch
        m_DownstreamIngest = m_Context->AllocBuffer(nop);
        if(m_DownstreamIngest == nullptr)
        {
          // can't allocate more so bail
          return false;
        }
      }
      m_DownstreamIngest->QueueTraffic(X, Y);
      return true;
    }

    void
    IHopHandler::CollectUpstream(AbstractRouter* r, Traffic_ptr batch)
    {
      if(m_UpstreamEgress.empty())
      {
        r->logic()->queue_func([self = GetSelf(), r] {
          while(not self->m_UpstreamEgress.empty())
          {
            Traffic_ptr t = self->m_UpstreamEgress.top();
            self->HandleAllUpstream(t, r);
            self->m_Context->FreeBuffer(t);
            self->m_UpstreamEgress.pop();
          }
          self->AfterCollectUpstream(r);
        });
      }
      m_UpstreamEgress.emplace(batch);
    }

    void
    IHopHandler::CollectDownstream(AbstractRouter* r, Traffic_ptr batch)
    {
      if(m_DownstreamEgress.empty())
      {
        r->logic()->queue_func([self = GetSelf(), r] {
          while(not self->m_DownstreamEgress.empty())
          {
            Traffic_ptr t = self->m_DownstreamEgress.top();
            self->HandleAllDownstream(t, r);
            self->m_Context->FreeBuffer(t);
            self->m_DownstreamEgress.pop();
          }
          self->AfterCollectDownstream(r);
        });
      }
      m_DownstreamEgress.emplace(batch);
    }

    void
    IHopHandler::FlushUpstream(AbstractRouter* r)
    {
      auto pool = m_UpstreamIngest;
      if(pool)
      {
        if(pool->empty())
        {
          m_Context->FreeBuffer(pool);
        }
        else
        {
          pool->seqno = m_UpstreamSequence++;
          if(not m_Context->QueuePathWork(
                 std::bind(&IHopHandler::UpstreamWork, GetSelf(), pool, r)))
          {
            m_Context->FreeBuffer(pool);
          }
        }
      }
      m_UpstreamIngest = nullptr;
    }

    void
    IHopHandler::FlushDownstream(AbstractRouter* r)
    {
      auto pool = m_DownstreamIngest;
      if(pool)
      {
        if(pool->empty())
        {
          m_Context->FreeBuffer(pool);
        }
        else
        {
          pool->seqno = m_DownstreamSequence++;
          if(not m_Context->QueuePathWork(
                 std::bind(&IHopHandler::DownstreamWork, GetSelf(), pool, r)))
          {
            m_Context->FreeBuffer(pool);
          }
        }
      }
      m_DownstreamIngest = nullptr;
    }

  }  // namespace path
}  // namespace llarp