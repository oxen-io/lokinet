#ifndef LLARP_CODEL_QUEUE_HPP
#define LLARP_CODEL_QUEUE_HPP
#ifdef _MSC_VER
#define NOMINMAX
#ifdef min
#undef min
#endif
#endif
#include <llarp/time.h>
#include <llarp/logger.hpp>
#include <llarp/mem.hpp>
#include <llarp/threading.hpp>

#include <algorithm>
#include <cmath>
#include <functional>

#include <array>
#include <string>

namespace llarp
{
  namespace util
  {
    struct DummyMutex
    {
    };

    struct DummyLock
    {
      DummyLock(const DummyMutex& mtx){};

      ~DummyLock()
      {
      }
    };

    template < typename T, typename GetTime, typename PutTime, typename Compare,
               typename Mutex_t = util::Mutex, typename Lock_t = util::Lock,
               llarp_time_t dropMs = 5, llarp_time_t initialIntervalMs = 100,
               size_t MaxSize = 1024 >
    struct CoDelQueue
    {
      CoDelQueue(const std::string& name) : m_name(name)
      {
      }

      size_t
      Size()
      {
        Lock_t lock(m_QueueMutex);
        return m_QueueIdx;
      }

      template < typename... Args >
      bool
      EmplaceIf(std::function< bool(T&) > pred, Args&&... args)
      {
        Lock_t lock(m_QueueMutex);
        if(m_QueueIdx == MaxSize)
          return false;
        T* t = &m_Queue[m_QueueIdx];
        new(t) T(std::forward< Args >(args)...);
        if(!pred(*t))
        {
          t->~T();
          return false;
        }

        PutTime()(m_Queue[m_QueueIdx]);
        if(firstPut == 0)
          firstPut = GetTime()(m_Queue[m_QueueIdx]);
        ++m_QueueIdx;

        return true;
      }

      template < typename... Args >
      void
      Emplace(Args&&... args)
      {
        Lock_t lock(m_QueueMutex);
        T* t = &m_Queue[m_QueueIdx];
        new(t) T(std::forward< Args >(args)...);
        PutTime()(m_Queue[m_QueueIdx]);
        if(firstPut == 0)
          firstPut = GetTime()(m_Queue[m_QueueIdx]);
        ++m_QueueIdx;
      }

      template < typename Visit >
      void
      Process(Visit visitor)
      {
        llarp_time_t lowest = 0xFFFFFFFFFFFFFFFFUL;
        // auto start          = llarp_time_now_ms();
        // llarp::LogInfo("CoDelQueue::Process - start at ", start);
        Lock_t lock(m_QueueMutex);
        auto start = firstPut;
        if(m_QueueIdx == 1)
        {
          visitor(m_Queue[0]);
          m_QueueIdx = 0;
          firstPut   = 0;
          return;
        }
        size_t idx = 0;
        while(m_QueueIdx)
        {
          llarp::LogDebug(m_name, " - queue has ", m_QueueIdx);
          T* item = &m_Queue[idx++];
          --m_QueueIdx;
          auto dlt = start - GetTime()(*item);
          // llarp::LogInfo("CoDelQueue::Process - dlt ", dlt);
          lowest = std::min(dlt, lowest);
          if(m_QueueIdx == 0)
          {
            // llarp::LogInfo("CoDelQueue::Process - single item: lowest ",
            // lowest, " dropMs: ", dropMs);
            if(lowest > dropMs)
            {
              llarp::LogInfo(m_name, " dropping");
              item->~T();
              nextTickInterval += initialIntervalMs / std::sqrt(++dropNum);
              firstPut = 0;
              return;
            }
            else
            {
              nextTickInterval = initialIntervalMs;
              dropNum          = 0;
            }
          }
          // llarp::LogInfo("CoDelQueue::Process - passing");
          visitor(*item);
          item->~T();
        }
        firstPut = 0;
      }

      llarp_time_t firstPut         = 0;
      size_t dropNum                = 0;
      llarp_time_t nextTickInterval = initialIntervalMs;
      Mutex_t m_QueueMutex;
      size_t m_QueueIdx = 0;
      T m_Queue[MaxSize];
      std::string m_name;
    };  // namespace util
  }     // namespace util
}  // namespace llarp

#endif
