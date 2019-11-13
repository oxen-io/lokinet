#ifndef LLARP_PATH_BUFFERS_HPP
#define LLARP_PATH_BUFFERS_HPP

#include <messages/relay.hpp>

namespace llarp
{
  namespace path
  {
    struct Batch
    {
      static constexpr size_t BatchSize = 32;
      Batch()                           = default;
      Batch(const Batch &)              = delete;
      Batch &
      operator=(const Batch &) = delete;
      Batch(Batch &&)          = delete;

      std::array< TrafficBuffer_t, BatchSize > _X;
      std::array< TunnelNonce, BatchSize > _Y;
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
          _X[_sz].Clear();
          _Y[_sz].Zero();
        }
        seqno = 0;
      }

      void
      QueueTraffic(const llarp_buffer_t &X, const TunnelNonce &Y)
      {
        if(IsFull())
          return;
        _X[_sz] = X;
        _Y[_sz] = Y;
        _sz++;
      }

      template < typename F >
      void
      ForEach(F &&func)
      {
        const std::function< void(TrafficBuffer_t &, TunnelNonce &) > f =
            std::move(func);
        for(size_t idx = 0; idx < _sz; ++idx)
          f(_X[idx], _Y[idx]);
      }

      using Ptr_t = Batch *;
    };

    using Traffic_t           = Batch;
    using Traffic_ptr         = Traffic_t::Ptr_t;
    using TrafficBufferPool_t = util::BufferPool< Traffic_t, false >;

  };  // namespace path
};    // namespace llarp
#endif