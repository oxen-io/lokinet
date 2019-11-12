#ifndef LLARP_PATH_BUFFERS_HPP
#define LLARP_PATH_BUFFERS_HPP

#include <messages/relay.hpp>

namespace llarp
{
  namespace path
  {
    template < typename T, size_t BatchSize = 128 >
    struct Batch
    {
      Batch()              = default;
      Batch(const Batch &) = delete;
      Batch &
      operator=(const Batch &) = delete;
      Batch(Batch &&)          = delete;

      std::array< T, BatchSize > _data;
      size_t _sz = 0;
      uint64_t seqno;

      size_t Index;

      bool
      operator<(const Batch &other) const
      {
        return other.seqno < seqno;
      }

      bool
      empty() const
      {
        return _sz == 0;
      }

      bool
      IsFull() const
      {
        return _sz >= BatchSize;
      }

      void
      Clear()
      {
        while(_sz > 0)
        {
          --_sz;
          _data[_sz].Clear();
        }
        seqno = 0;
      }

      void
      GiveMessageAt(const llarp_buffer_t &X, const TunnelNonce &Y, size_t idx)
      {
        if(IsFull())
          return;
        _data[idx].X = X;
        _data[idx].Y = Y;
        _sz++;
      }

      template < typename F >
      void
      ForEach(F &&f)
      {
        for(size_t idx = 0; idx < _sz; ++idx)
          f(_data[idx]);
      }

      using Ptr_t = Batch< T > *;
    };

    using UpstreamTraffic_t     = Batch< RelayUpstreamMessage >;
    using UpstreamTraffic_ptr   = UpstreamTraffic_t::Ptr_t;
    using DownstreamTraffic_t   = Batch< RelayDownstreamMessage >;
    using DownstreamTraffic_ptr = DownstreamTraffic_t::Ptr_t;
    using UpstreamBufferPool_t  = util::BufferPool< UpstreamTraffic_t, false >;
    using DownstreamBufferPool_t =
        util::BufferPool< DownstreamTraffic_t, false >;

    struct MemPool_Impl;

    struct MemPool
    {
      MemPool();
      ~MemPool();

      MemPool(const MemPool &) = delete;
      MemPool(MemPool &&)      = delete;

      MemPool &
      operator=(const MemPool &) = delete;
      MemPool &
      operator=(MemPool &&) = delete;

      UpstreamBufferPool_t::Ptr_t
      ObtainUpstreamBufferPool();

      void ReturnUpstreamBufferPool(UpstreamBufferPool_t::Ptr_t);

      DownstreamBufferPool_t::Ptr_t
      ObtainDownstreamBufferPool();

      void ReturnDownstreamBufferPool(DownstreamBufferPool_t::Ptr_t);

      void
      Cleanup();

     private:
      MemPool_Impl *m_Impl;
    };

  };  // namespace path
};    // namespace llarp
#endif