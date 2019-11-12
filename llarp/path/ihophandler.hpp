#ifndef LLARP_PATH_IHOPHANDLER_HPP
#define LLARP_PATH_IHOPHANDLER_HPP

#include <path/path_types.hpp>
#include <util/types.hpp>
#include <util/compare_ptr.hpp>
#include <crypto/encrypted_frame.hpp>
#include <path/path_buffers.hpp>
#include <vector>
#include <memory>

struct llarp_buffer_t;

namespace llarp
{
  struct AbstractRouter;

  namespace routing
  {
    struct IMessage;
  }

  namespace path
  {
    struct IHopHandler
    {
      using UpstreamQueue_t =
          std::priority_queue< UpstreamTraffic_ptr,
                               std::vector< UpstreamTraffic_ptr >,
                               llarp::ComparePtr< UpstreamTraffic_ptr > >;
      using DownstreamQueue_t =
          std::priority_queue< DownstreamTraffic_ptr,
                               std::vector< DownstreamTraffic_ptr >,
                               llarp::ComparePtr< DownstreamTraffic_ptr > >;

      virtual ~IHopHandler() = default;

      virtual bool
      Expired(llarp_time_t now) const = 0;

      virtual bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const = 0;

      /// called to kill this ihop handler
      /// any further use on it will not fowrard or process traffiy any more
      virtual void
      Destroy() = 0;

      /// send routing message and increment sequence number
      virtual bool
      SendRoutingMessage(const routing::IMessage& msg, AbstractRouter* r) = 0;

      // handle data in upstream direction
      bool
      HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                     AbstractRouter*);
      // handle data in downstream direction
      bool
      HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                       AbstractRouter*);

      /// return timestamp last remote activity happened at
      virtual llarp_time_t
      LastRemoteActivityAt() const = 0;

      virtual bool
      HandleLRSM(uint64_t status, std::array< EncryptedFrame, 8 >& frames,
                 AbstractRouter* r) = 0;

      uint64_t
      NextSeqNo()
      {
        return m_SequenceNum++;
      }

      void
      FlushUpstream(AbstractRouter* r);

      void
      FlushDownstream(AbstractRouter* r);

      /// get downstream router id
      virtual const RouterID
      Downstream() const = 0;

      /// get upstream router id
      virtual const RouterID
      Upstream() const = 0;

      virtual const PathID_t
      TXID() const = 0;

      virtual const PathID_t
      RXID() const = 0;

      struct Hash
      {
        size_t
        operator()(const IHopHandler& p) const
        {
          const auto tx      = p.TXID();
          const auto rx      = p.RXID();
          const auto u       = p.Upstream();
          const auto d       = p.Downstream();
          const size_t rhash = std::accumulate(
              u.begin(), u.end(),
              std::accumulate(d.begin(), d.end(), 0, std::bit_xor< size_t >()),
              std::bit_xor< size_t >());
          return std::accumulate(rx.begin(), rx.begin(),
                                 std::accumulate(tx.begin(), tx.end(), rhash,
                                                 std::bit_xor< size_t >()),
                                 std::bit_xor< size_t >());
        }
      };

      struct Ptr_Hash
      {
        size_t
        operator()(const std::shared_ptr< IHopHandler >& p) const
        {
          if(p == nullptr)
            return 0;
          return Hash{}(*p);
        }
      };

     protected:
      uint64_t m_SequenceNum                   = 0;
      UpstreamTraffic_ptr m_UpstreamIngest     = nullptr;
      DownstreamTraffic_ptr m_DownstreamIngest = nullptr;
      uint64_t m_UpstreamSequence              = 0;
      uint64_t m_DownstreamSequence            = 0;
      UpstreamQueue_t m_UpstreamEgress;
      DownstreamQueue_t m_DownstreamEgress;

      virtual void
      AfterCollectUpstream(AbstractRouter* r) = 0;

      virtual void
      AfterCollectDownstream(AbstractRouter* r) = 0;

      void
      CollectDownstream(AbstractRouter* r, DownstreamTraffic_ptr data);

      void
      CollectUpstream(AbstractRouter* r, UpstreamTraffic_ptr data);

      virtual void
      UpstreamWork(UpstreamTraffic_ptr queue, AbstractRouter* r) = 0;

      virtual void
      DownstreamWork(DownstreamTraffic_ptr queue, AbstractRouter* r) = 0;

      virtual void
      HandleAllUpstream(UpstreamTraffic_ptr msgs, AbstractRouter* r) = 0;
      virtual void
      HandleAllDownstream(DownstreamTraffic_ptr msgs, AbstractRouter* r) = 0;

      virtual std::shared_ptr< IHopHandler >
      GetSelf() = 0;

      /// get the upstream message buffer pool for this ihop handler
      virtual UpstreamBufferPool_t::Ptr_t
      ObtainUpstreamBufferPool() = 0;

      /// get the downstream message buffer pool for this ihop handler
      virtual DownstreamBufferPool_t::Ptr_t
      ObtainDownstreamBufferPool() = 0;

      virtual void ReturnUpstreamBufferPool(UpstreamBufferPool_t::Ptr_t) = 0;

      virtual void ReturnDownstreamBufferPool(
          DownstreamBufferPool_t::Ptr_t) = 0;

      UpstreamBufferPool_t::Ptr_t m_UpstreamPool     = nullptr;
      DownstreamBufferPool_t::Ptr_t m_DownstreamPool = nullptr;

     private:
      UpstreamTraffic_ptr
      NewUpstream();

      void FreeUpstream(UpstreamTraffic_ptr);

      DownstreamTraffic_ptr
      NewDownstream();

      void FreeDownstream(DownstreamTraffic_ptr);
    };

    using HopHandler_ptr = std::shared_ptr< IHopHandler >;
  }  // namespace path
}  // namespace llarp
#endif
