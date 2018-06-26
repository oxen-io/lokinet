#ifndef LLARP_CODEL_QUEUE_HPP
#define LLARP_CODEL_QUEUE_HPP
#include <llarp/time.h>
#include <cmath>
#include <functional>
#include <mutex>
#include <queue>

namespace llarp
{
  namespace util
  {
    template < typename T, typename GetTime, llarp_time_t dropMs = 5,
               llarp_time_t initialIntervalMs = 100 >
    struct CoDelQueue
    {
      struct CoDelCompare
      {
        GetTime getTime = GetTime();
        bool
        operator()(const T& left, const T& right) const
        {
          return getTime(left) < getTime(right);
        }
      };

      void
      Put(T* item)
      {
        std::unique_lock< std::mutex > lock(m_QueueMutex);
        m_Queue.push(*item);
      }

      void
      Process(std::queue< T >& result)
      {
        llarp_time_t lowest = 0xFFFFFFFFFFFFFFFFUL;
        auto start          = llarp_time_now_ms();
        std::unique_lock< std::mutex > lock(m_QueueMutex);
        while(m_Queue.size())
        {
          const auto& item = m_Queue.top();
          auto dlt         = start - getTime(item);
          lowest           = std::min(dlt, lowest);
          if(m_Queue.size() == 1)
          {
            if(lowest > dropMs)
            {
              // drop
              nextTickInterval += initialIntervalMs / std::sqrt(++dropNum);
              llarp::Info("CoDel drop ", nextTickInterval, " ms next interval");
              m_Queue.pop();
              return;
            }
            else
            {
              nextTickInterval = initialIntervalMs;
              dropNum          = 0;
            }
          }
          result.push(item);
          m_Queue.pop();
        }
      }

      GetTime getTime               = GetTime();
      size_t dropNum                = 0;
      llarp_time_t nextTickInterval = initialIntervalMs;
      std::mutex m_QueueMutex;
      std::priority_queue< T, std::vector< T >, CoDelCompare > m_Queue;
    };
  }  // namespace util
}  // namespace llarp

#endif