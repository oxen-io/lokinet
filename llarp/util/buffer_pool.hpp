#ifndef LLARP_UTIL_BUFFER_POOL_HPP
#define LLARP_UTIL_BUFFER_POOL_HPP

#include <absl/types/optional.h>
#include <util/thread/queue.hpp>
#include <util/thread/threading.hpp>
#include <bitset>
#include <array>

namespace llarp
{
  namespace util
  {
    template < typename T, bool multithread = true, size_t Buckets = 32 >
    struct BufferPool
    {
      static constexpr size_t Buffers = Buckets;
      using Ptr_t                     = BufferPool*;

      BufferPool(size_t _sz) : m_FreeQueue(_sz)
      {
        size_t idx = 0;
        for(auto& buf : m_Buffers)
        {
          buf.Clear();
          buf.Index = idx;
          ++idx;
        }
        m_BuffersAllocated.reset();
        m_FreeQueue.enable();
      }

      virtual ~BufferPool() = default;

      void
      FreeBuffer(T* t)
      {
        if(t == nullptr)
          return;
        Call< void >([&]() {
          assert(m_BuffersAllocated.test(t->Index));
          if(not multithread)
          {
            if(m_FreeQueue.full())
            {
              DrainBuffers();
            }
          }
          m_FreeQueue.pushBack(t);
        });
      }

      T*
      AllocBuffer(std::function< bool(T&) > full)
      {
        if(multithread)
        {
          m_BuffersAllocated.set(m_BufferIndex);
          auto& bucket = m_Buffers[m_BufferIndex];
          if(full(bucket))
          {
            return nullptr;
          }
          else
          {
            return &bucket;
          }
        }
        else
        {
          return Call< T* >([&]() { return PopNextBuffer(); });
        }
      }

      template < typename F >
      void
      UseCurrentBuffer(F f)
      {
        if(multithread)
        {
          f(m_Buffers[m_BufferIndex], m_BucketPosition);
          m_BucketPosition++;
        }
      }

      T*
      PopNextBuffer()
      {
        const auto idx = m_BufferIndex;
        T* ptr         = nullptr;
        if(multithread)
        {
          if(m_BucketPosition)
          {
            m_BucketPosition = 0;
            ptr              = &m_Buffers[idx];
          }
          else
          {
            m_Buffers[idx].Clear();
            m_BuffersAllocated.reset(idx);
          }

          if(ptr)
          {
            // advance to next good buffer
            do
            {
              m_BufferIndex = (m_BufferIndex + 1) % Buckets;
              if(m_BufferIndex == idx)
                break;
            } while(m_BuffersAllocated.test(m_BufferIndex));
          }
        }
        else
        {
          // find good bucket
          while(m_BuffersAllocated.test(m_BufferIndex))
          {
            m_BufferIndex = (m_BufferIndex + 1) % Buckets;
            // looks full?
            if(m_BufferIndex == idx)
              return nullptr;
          }
          // acquire bucket
          assert(not m_BuffersAllocated.test(m_BufferIndex));
          m_BuffersAllocated.set(m_BufferIndex);
          ptr = &m_Buffers[m_BufferIndex];
          ptr->Clear();
        }
        return ptr;
      }

      void
      EnsureBufferSpace()
      {
        Call< void >([&]() {
          DrainBuffers();
          auto idx = FindNextGoodBufferIndex();
          if(idx.has_value())
          {
            m_BufferIndex = idx.value();
          }
        });
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

     protected:
      void
      EndAllocator()
      {
        m_FreeQueue.disable();
      }

      std::array< T, Buckets > m_Buffers;

     private:
      void
      DrainBuffers()
      {
        do
        {
          auto val = m_FreeQueue.tryPopFront();
          if(not val.has_value())
            return;
          // mark this buffer as free
          const size_t idx = val.value()->Index;
          assert(m_BuffersAllocated.test(idx));
          m_Buffers[idx].Clear();
          m_BuffersAllocated.reset(idx);
        } while(true);
      }

      using BufferLock_t  = NullLock;
      using BufferMutex_t = NullMutex;
      llarp::thread::Queue< T* > m_FreeQueue;
      std::bitset< Buckets > m_BuffersAllocated;
      size_t m_BucketPosition = 0;
      size_t m_BufferIndex    = 0;
      BufferMutex_t m_Access;

      template < typename Val_t >
      Val_t
      Call(std::function< Val_t(void) > f)
      {
        if(multithread)
        {
          return f();
        }
        else
        {
          BufferLock_t lock(&m_Access);
          return f();
        }
      }
    };
  }  // namespace util
}  // namespace llarp

#endif