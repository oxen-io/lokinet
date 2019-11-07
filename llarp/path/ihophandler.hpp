#ifndef LLARP_PATH_IHOPHANDLER_HPP
#define LLARP_PATH_IHOPHANDLER_HPP

#include <crypto/types.hpp>
#include <util/types.hpp>
#include <crypto/encrypted_frame.hpp>
#include <messages/relay.hpp>
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
      using TrafficEvent_t = std::pair< std::vector< byte_t >, TunnelNonce >;

      template < typename T >
      struct Batch
      {
        std::vector< T > msgs;
        uint64_t seqno;

        bool
        operator<(const Batch& other) const
        {
          return seqno < other.seqno;
        }

        bool
        empty() const
        {
          return msgs.empty();
        }
      };

      using TrafficQueue_t   = Batch< TrafficEvent_t >;
      using TrafficQueue_ptr = std::shared_ptr< TrafficQueue_t >;

      using UpstreamQueue_t =
          std::priority_queue< Batch< RelayUpstreamMessage > >;
      using DownstreamQueue_t =
          std::priority_queue< Batch< RelayDownstreamMessage > >;

      virtual ~IHopHandler() = default;

      virtual bool
      Expired(llarp_time_t now) const = 0;

      virtual bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const = 0;

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

     protected:
      uint64_t m_SequenceNum = 0;
      TrafficQueue_ptr m_UpstreamIngest;
      TrafficQueue_ptr m_DownstreamIngest;
      uint64_t m_UpstreamSequence   = 0;
      uint64_t m_DownstreamSequence = 0;

      std::priority_queue< Batch< RelayUpstreamMessage > > m_UpstreamEgress;
      std::priority_queue< Batch< RelayDownstreamMessage > > m_DownstreamEgress;

      virtual void
      AfterCollectUpstream(AbstractRouter* r) = 0;

      virtual void
      AfterCollectDownstream(AbstractRouter* r) = 0;

      void
      CollectDownstream(AbstractRouter* r,
                        Batch< RelayDownstreamMessage > data);

      void
      CollectUpstream(AbstractRouter* r, Batch< RelayUpstreamMessage > data);

      virtual void
      UpstreamWork(TrafficQueue_ptr queue, AbstractRouter* r) = 0;

      virtual void
      DownstreamWork(TrafficQueue_ptr queue, AbstractRouter* r) = 0;

      virtual void
      HandleAllUpstream(const std::vector< RelayUpstreamMessage >& msgs,
                        AbstractRouter* r) = 0;
      virtual void
      HandleAllDownstream(const std::vector< RelayDownstreamMessage >& msgs,
                          AbstractRouter* r) = 0;

      virtual std::shared_ptr< IHopHandler >
      GetSelf() = 0;
    };

    using HopHandler_ptr = std::shared_ptr< IHopHandler >;
  }  // namespace path
}  // namespace llarp
#endif
