#ifndef LLARP_UTIL_BUFFER_POOL_HPP
#define LLARP_UTIL_BUFFER_POOL_HPP

#include <absl/types/optional.h>
#include <util/thread/queue.hpp>
#include <bitset>
#include <array>

namespace llarp
{
  namespace util
  {
    template < typename T, bool multithread = true, size_t Buckets = 1024 >
    struct BufferPool
    {
      size_t Index = 0;

      using Ptr_t = BufferPool*;

      BufferPool(size_t sz = Buckets / 4) : m_FreeQueue(sz)
      {
        size_t idx = 0;
        for(auto& buf : m_Buffers)
        {
          buf.Index = idx;
          ++idx;
        }
        m_BuffersAllocated.reset();
      }

      void
      FreeBuffer(T* t)
      {
        if(t == nullptr)
          return;
        while(m_FreeQueue.tryPushBack(t->Index)
              == llarp::thread::QueueReturn::QueueFull)
        {
          if(multithread)
          {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
          }
          else
          {
            DrainBuffers();
          }
        }
      }

      T*
      AllocBuffer(std::function< bool(T&) > full)
      {
        m_BuffersAllocated.set(m_BufferIndex);
        auto& bucket = m_Buffers[m_BufferIndex];
        if(full(bucket))
        {
          return nullptr;
        }
        else
        {
          m_BuffersAllocated.set(m_BufferIndex);
          return &m_Buffers[m_BufferIndex];
        }
      }

      template < typename F >
      void
      UseCurrentBuffer(F&& f)
      {
        f(m_Buffers[m_BufferIndex], m_BucketPosition);
        m_BucketPosition++;
      }

      T*
      PopNextBuffer()
      {
        const auto idx = m_BufferIndex;
        // find a good bucket
        do
        {
          m_BufferIndex = (m_BufferIndex + 1) % Buckets;
        } while(m_BuffersAllocated.test(m_BufferIndex) && m_BufferIndex != idx);
        if(m_BufferIndex == idx)
        {
          // full
          EnsureBufferSpace();
        }
        if(m_BucketPosition == 0)
          return nullptr;
        m_BucketPosition = 0;
        return &m_Buffers[idx];
      }

      /// cease all allocations
      void
      Finish()
      {
        DrainBuffers();
        EndAllocator();
      }

      void
      EnsureBufferSpace()
      {
        DrainBuffers();
        auto idx = FindNextGoodBufferIndex();
        if(idx.has_value())
        {
          m_BufferIndex = idx.value();
        }
      }

      const T&
      Current() const
      {
        return m_Buffers[m_BufferIndex];
      }

     private:
      absl::optional< size_t >
      FindNextGoodBufferIndex() const
      {
        size_t idx = 0;
        while(idx < Buckets)
        {
          if(not m_BuffersAllocated.test(idx))
            return idx;
          ++idx;
        }
        return {};
      }

      void
      DrainBuffers()
      {
        do
        {
          auto val = m_FreeQueue.tryPopFront();
          if(not val.has_value())
            return;
          // mark this buffer as free
          const size_t idx = val.value();
          m_BuffersAllocated.reset(idx);
          m_Buffers[idx].Clear();
        } while(true);
      }

     protected:
      void
      EndAllocator()
      {
        m_FreeQueue.disable();
      }

      std::array< T, Buckets > m_Buffers;

     private:
      llarp::thread::Queue< size_t > m_FreeQueue;
      std::bitset< Buckets > m_BuffersAllocated;
      size_t m_BucketPosition = 0;
      size_t m_BufferIndex    = 0;
    };
  }  // namespace util
}  // namespace llarp

#endif