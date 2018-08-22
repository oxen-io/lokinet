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

#include <queue>
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
    template < typename T, typename GetTime >
    struct CoDelCompareTime
    {
      bool
      operator()(const T& left, const T& right) const
      {
        return GetTime()(left) < GetTime()(right);
      }
    };

    template < typename T >
    struct CoDelComparePriority
    {
      bool
      operator()(const T& left, const T& right) const
      {
        return left < right;
      }
    };

    template < typename T, typename GetTime, typename PutTime, typename Compare,
               typename Mutex_t = util::Mutex, typename Lock_t = util::Lock,
               llarp_time_t dropMs = 5, llarp_time_t initialIntervalMs = 100 >
    struct CoDelQueue
    {
      CoDelQueue(const std::string& name) : m_name(name)
      {
      }

      size_t
      Size()
      {
        Lock_t lock(m_QueueMutex);
        return m_Queue.size();
      }

      template < typename... Args >
      bool
      EmplaceIf(std::function< bool(T*) > pred, Args&&... args)
      {
        T* ptr = new T(std::forward< Args >(args)...);
        if(!pred(ptr))
        {
          delete ptr;
          return false;
        }
        PutTime()(ptr);
        {
          Lock_t lock(m_QueueMutex);
          if(firstPut == 0)
            firstPut = GetTime()(ptr);
          m_Queue.push(ptr);
        }
        return true;
      }

      template < typename... Args >
      void
      Emplace(Args&&... args)
      {
        T* ptr = new T(std::forward< Args >(args)...);
        PutTime()(ptr);
        {
          Lock_t lock(m_QueueMutex);
          if(firstPut == 0)
            firstPut = GetTime()(ptr);
          m_Queue.push(ptr);
        }
      }

      void
      Put(T* ptr)
      {
        PutTime()(ptr);
        {
          Lock_t lock(m_QueueMutex);
          if(firstPut == 0)
            firstPut = GetTime()(ptr);
          m_Queue.push(ptr);
        }
      }

      template < typename Q >
      void
      _sort(Q& queue)
      {
        /*
        std::vector< std::unique_ptr< T > > q;
        while(queue.size())
        {
          q.emplace_back(std::move(queue.front()));
          queue.pop();
        }
        std::sort(q.begin(), q.end(), Compare());
        auto itr = q.begin();
        while(itr != q.end())
        {
          queue.push(std::move(*itr));
          ++itr;
        }
        */
      }

      /// visit returns true to discard entry otherwise the entry is
      /// re quened
      template < typename Visit >
      void
      ProcessIf(Visit visitor)
      {
        llarp_time_t lowest = 0xFFFFFFFFFFFFFFFFUL;
        // auto start          = llarp_time_now_ms();
        // llarp::LogInfo("CoDelQueue::Process - start at ", start);
        Lock_t lock(m_QueueMutex);
        _sort(m_Queue);
        auto start = firstPut;
        std::queue< T* > requeue;
        while(m_Queue.size())
        {
          llarp::LogDebug("CoDelQueue::Process - queue has ", m_Queue.size());
          T* item  = m_Queue.front();
          auto dlt = start - GetTime()(item);
          // llarp::LogInfo("CoDelQueue::Process - dlt ", dlt);
          lowest = std::min(dlt, lowest);
          if(m_Queue.size() == 1)
          {
            // llarp::LogInfo("CoDelQueue::Process - single item: lowest ",
            // lowest, " dropMs: ", dropMs);
            if(lowest > dropMs)
            {
              nextTickInterval += initialIntervalMs / std::sqrt(++dropNum);
              m_Queue.pop();
              delete item;
              break;
            }
            else
            {
              nextTickInterval = initialIntervalMs;
              dropNum          = 0;
            }
          }
          // llarp::LogInfo("CoDelQueue::Process - passing");
          if(!visitor(item))
          {
            // requeue item as we are not done
            requeue.push(item);
          }
          else
          {
            delete item;
          }
          m_Queue.pop();
        }
        m_Queue  = std::move(requeue);
        firstPut = 0;
      }

      template < typename Func >
      void
      Process(Func visitor)
      {
        ProcessIf([visitor](T* t) -> bool {
          visitor(t);
          return true;
        });
      }

      llarp_time_t firstPut         = 0;
      size_t dropNum                = 0;
      llarp_time_t nextTickInterval = initialIntervalMs;
      Mutex_t m_QueueMutex;
      std::queue< T* > m_Queue;
      std::string m_name;
    };
  }  // namespace util
}  // namespace llarp

#endif
