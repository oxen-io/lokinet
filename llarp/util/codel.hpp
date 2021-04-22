#pragma once

#include <llarp/util/logging/logger.hpp>
#include "mem.hpp"
#include <llarp/util/thread/threading.hpp>
#include "time.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace llarp
{
  namespace util
  {
    struct GetNowSyscall
    {
      llarp_time_t
      operator()() const
      {
        return llarp::time_now_ms();
      }
    };

    template <
        typename T,
        typename GetTime,
        typename PutTime,
        typename Compare,
        typename GetNow = GetNowSyscall,
        typename Mutex_t = util::Mutex,
        typename Lock_t = std::lock_guard<Mutex_t>,
        size_t MaxSize = 1024>
    struct CoDelQueue
    {
      CoDelQueue(std::string name, PutTime put, GetNow now)
          : m_QueueIdx(0)
          , m_name(std::move(name))
          , _putTime(std::move(put))
          , _getNow(std::move(now))
      {}

      size_t
      Size() EXCLUDES(m_QueueMutex)
      {
        Lock_t lock(m_QueueMutex);
        return m_QueueIdx;
      }

      template <typename... Args>
      bool
      EmplaceIf(std::function<bool(T&)> pred, Args&&... args) EXCLUDES(m_QueueMutex)
      {
        Lock_t lock(m_QueueMutex);
        if (m_QueueIdx == MaxSize)
          return false;
        T* t = &m_Queue[m_QueueIdx];
        new (t) T(std::forward<Args>(args)...);
        if (!pred(*t))
        {
          t->~T();
          return false;
        }

        _putTime(m_Queue[m_QueueIdx]);
        if (firstPut == 0s)
          firstPut = _getTime(m_Queue[m_QueueIdx]);
        ++m_QueueIdx;

        return true;
      }

      template <typename... Args>
      void
      Emplace(Args&&... args) EXCLUDES(m_QueueMutex)
      {
        Lock_t lock(m_QueueMutex);
        if (m_QueueIdx == MaxSize)
          return;
        T* t = &m_Queue[m_QueueIdx];
        new (t) T(std::forward<Args>(args)...);
        _putTime(m_Queue[m_QueueIdx]);
        if (firstPut == 0s)
          firstPut = _getTime(m_Queue[m_QueueIdx]);
        ++m_QueueIdx;
      }

      template <typename Visit>
      void
      Process(Visit v)
      {
        return Process(v, [](T&) -> bool { return false; });
      }

      template <typename Visit, typename Filter>
      void
      Process(Visit visitor, Filter f) EXCLUDES(m_QueueMutex)
      {
        llarp_time_t lowest = std::numeric_limits<llarp_time_t>::max();
        if (_getNow() < nextTickAt)
          return;
        // llarp::LogInfo("CoDelQueue::Process - start at ", start);
        Lock_t lock(m_QueueMutex);
        auto start = firstPut;

        if (m_QueueIdx == 1)
        {
          visitor(m_Queue[0]);
          T* t = &m_Queue[0];
          t->~T();
          m_QueueIdx = 0;
          firstPut = 0s;
          return;
        }
        size_t idx = 0;
        while (m_QueueIdx)
        {
          llarp::LogDebug(m_name, " - queue has ", m_QueueIdx);
          T* item = &m_Queue[idx++];
          if (f(*item))
            break;
          --m_QueueIdx;
          const llarp_time_t dlt = start - _getTime(*item);
          // llarp::LogInfo("CoDelQueue::Process - dlt ", dlt);
          lowest = std::min(dlt, lowest);
          if (m_QueueIdx == 0)
          {
            // llarp::LogInfo("CoDelQueue::Process - single item: lowest ",
            // lowest, " dropMs: ", dropMs);
            if (lowest > dropMs)
            {
              item->~T();
              nextTickInterval += initialIntervalMs / uint64_t(std::sqrt(++dropNum));
              firstPut = 0s;
              nextTickAt = start + nextTickInterval;
              return;
            }

            nextTickInterval = initialIntervalMs;
            dropNum = 0;
          }
          visitor(*item);
          item->~T();
        }
        firstPut = 0s;
        nextTickAt = start + nextTickInterval;
      }

      const llarp_time_t initialIntervalMs = 5ms;
      const llarp_time_t dropMs = 100ms;
      llarp_time_t firstPut = 0s;
      size_t dropNum = 0;
      llarp_time_t nextTickInterval = initialIntervalMs;
      llarp_time_t nextTickAt = 0s;
      Mutex_t m_QueueMutex;
      size_t m_QueueIdx GUARDED_BY(m_QueueMutex);
      std::array<T, MaxSize> m_Queue GUARDED_BY(m_QueueMutex);
      std::string m_name;
      GetTime _getTime;
      PutTime _putTime;
      GetNow _getNow;
    };  // namespace util
  }     // namespace util
}  // namespace llarp
