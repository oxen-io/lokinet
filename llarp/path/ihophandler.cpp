#include <path/ihophandler.hpp>
#include <router/abstractrouter.hpp>

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
        m_UpstreamIngest        = std::make_shared< TrafficQueue_t >();
        m_UpstreamIngest->seqno = m_UpstreamSequence++;
      }
      m_UpstreamIngest->msgs.emplace_back();
      auto& pkt = m_UpstreamIngest->msgs.back();
      pkt.first.resize(X.sz);
      std::copy_n(X.base, X.sz, pkt.first.begin());
      pkt.second = Y;
      return true;
    }

    // handle data in downstream direction
    bool
    IHopHandler::HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                                  AbstractRouter*)
    {
      if(m_DownstreamIngest == nullptr)
      {
        m_DownstreamIngest        = std::make_shared< TrafficQueue_t >();
        m_DownstreamIngest->seqno = m_DownstreamSequence++;
      }
      m_DownstreamIngest->msgs.emplace_back();
      auto& pkt = m_DownstreamIngest->msgs.back();
      pkt.first.resize(X.sz);
      std::copy_n(X.base, X.sz, pkt.first.begin());
      pkt.second = Y;
      return true;
    }

    void
    IHopHandler::CollectUpstream(AbstractRouter* r,
                                 Batch< RelayUpstreamMessage > batch)
    {
      if(m_UpstreamEgress.empty())
      {
        r->logic()->queue_func([self = GetSelf(), r] {
          while(not self->m_UpstreamEgress.empty())
          {
            const auto& top = self->m_UpstreamEgress.top();
            self->HandleAllUpstream(top.msgs, r);
            self->m_UpstreamEgress.pop();
          }
          r->linkManager().PumpLinks();
        });
      }
      m_UpstreamEgress.emplace(std::move(batch));
    }

    void
    IHopHandler::CollectDownstream(AbstractRouter* r,
                                   Batch< RelayDownstreamMessage > batch)
    {
      if(m_DownstreamEgress.empty())
      {
        r->logic()->queue_func([self = GetSelf(), r] {
          while(not self->m_DownstreamEgress.empty())
          {
            const auto& top = self->m_DownstreamEgress.top();
            self->HandleAllDownstream(top.msgs, r);
            self->m_DownstreamEgress.pop();
          }
          r->linkManager().PumpLinks();
        });
      }
      m_DownstreamEgress.emplace(std::move(batch));
    }

    void
    IHopHandler::FlushUpstream(AbstractRouter* r)
    {
      if(m_UpstreamIngest && !m_UpstreamIngest->empty())
        r->threadpool()->addJob(std::bind(&IHopHandler::UpstreamWork, GetSelf(),
                                          std::move(m_UpstreamIngest), r));

      m_UpstreamIngest = nullptr;
    }

    void
    TransitHop::FlushDownstream(AbstractRouter* r)
    {
      if(m_DownstreamIngest && !m_DownstreamIngest->empty())
        r->threadpool()->addJob(std::bind(&IHopHandler::DownstreamWork,
                                          GetSelf(),
                                          std::move(m_DownstreamIngest), r));
      m_DownstreamIngest = nullptr;
    }

  }  // namespace path
}  // namespace llarp