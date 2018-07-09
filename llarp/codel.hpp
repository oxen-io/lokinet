#ifndef LLARP_CODEL_QUEUE_HPP
#define LLARP_CODEL_QUEUE_HPP
#include <llarp/time.h>
#include "llarp/logger.hpp"

#include <cmath>
#include <functional>
#include <mutex>
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

    template < typename T, typename GetTime, typename PutTime,
               typename Mutex_t    = std::mutex,
               typename Lock_t     = std::unique_lock< Mutex_t >,
               llarp_time_t dropMs = 20, llarp_time_t initialIntervalMs = 100 >
    struct CoDelQueue
    {
      CoDelQueue(const std::string& name) : m_name(name)
      {
      }

      struct CoDelCompare
      {
        bool
        operator()(const T& left, const T& right) const
        {
          return GetTime()(left) < GetTime()(right);
        }
      };

      size_t
      Size()
      {
        Lock_t lock(m_QueueMutex);
        return m_Queue.size();
      }

      void
      Put(const T& i)
      {
        Lock_t lock(m_QueueMutex);
        // llarp::LogInfo("CoDelQueue::Put - adding item, queue now has ",
        // m_Queue.size(), " items at ", getTime(*item));
        PutTime()(i);
        m_Queue.push(i);
        if(firstPut == 0)
          firstPut = GetTime()(i);
      }

      template < typename Queue_t >
      void
      Process(Queue_t& result)
      {
        llarp_time_t lowest = 0xFFFFFFFFFFFFFFFFUL;
        // auto start          = llarp_time_now_ms();
        // llarp::LogInfo("CoDelQueue::Process - start at ", start);
        Lock_t lock(m_QueueMutex);
        auto start = firstPut;
        while(m_Queue.size())
        {
          // llarp::LogInfo("CoDelQueue::Process - queue has ", m_Queue.size());
          const auto& item = m_Queue.top();
          auto dlt         = start - GetTime()(item);
          // llarp::LogInfo("CoDelQueue::Process - dlt ", dlt);
          lowest = std::min(dlt, lowest);
          if(m_Queue.size() == 1)
          {
            // llarp::LogInfo("CoDelQueue::Process - single item: lowest ",
            // lowest, " dropMs: ", dropMs);
            if(lowest > dropMs)
            {
              // drop
              nextTickInterval += initialIntervalMs / std::sqrt(++dropNum);
              llarp::LogWarn("CoDel queue ", m_name, " drop ", nextTickInterval,
                             " ms next interval lowest=", lowest);
              delete item;
              m_Queue.pop();
              break;
            }
            else
            {
              nextTickInterval = initialIntervalMs;
              dropNum          = 0;
            }
          }
          // llarp::LogInfo("CoDelQueue::Process - passing");
          result.push(item);
          m_Queue.pop();
        }
        firstPut = 0;
      }

      llarp_time_t firstPut         = 0;
      size_t dropNum                = 0;
      llarp_time_t nextTickInterval = initialIntervalMs;
      Mutex_t m_QueueMutex;
      std::priority_queue< T, std::vector< T >, CoDelCompare > m_Queue;
      std::string m_name;
    };
  }  // namespace util
}  // namespace llarp

#endif
