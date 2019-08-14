#include <util/scheduler.hpp>
#include <utility>

namespace llarp
{
  namespace thread
  {
    const Scheduler::Handle Scheduler::INVALID_HANDLE = -1;

    void
    Scheduler::dispatch()
    {
      using PendingRepeatItem = TimerQueueItem< RepeatDataPtr >;

      std::vector< PendingRepeatItem > pendingRepeats;

      while(true)
      {
        {
          util::Lock l(&m_mutex);

          if(!m_running.load(std::memory_order_relaxed))
          {
            return;
          }

          m_iterationCount++;

          size_t newRepeatSize = 0, newEventSize = 0;

          absl::Time now = m_clock();

          static constexpr size_t MAX_PENDING_REPEAT = 64;
          static constexpr size_t MAX_PENDING_EVENTS = 64;

          absl::Time minRepeat, minEvent;

          m_repeatQueue.popLess(now, MAX_PENDING_REPEAT, &pendingRepeats,
                                &newRepeatSize, &minRepeat);

          m_eventQueue.popLess(now, MAX_PENDING_EVENTS, &m_events,
                               &newEventSize, &minEvent);

          // If there are no pending events to process...
          if(pendingRepeats.empty() && m_events.empty())
          {
            // if there are none in the queue *at all* block until woken
            if(newRepeatSize == 0 && newEventSize == 0)
            {
              m_condition.Wait(&m_mutex);
            }
            else
            {
              absl::Time minTime;

              if(newRepeatSize == 0)
              {
                minTime = minEvent;
              }
              else if(newEventSize == 0)
              {
                minTime = minRepeat;
              }
              else
              {
                minTime = std::min(minRepeat, minEvent);
              }

              m_condition.WaitWithDeadline(&m_mutex, minTime);
            }

            continue;
          }
        }

        auto repeatIt = pendingRepeats.begin();
        m_eventIt     = m_events.begin();

        while(repeatIt != pendingRepeats.end() && m_eventIt != m_events.end())
        {
          auto repeatTime = repeatIt->time();
          auto eventTime  = m_eventIt->time();

          if(repeatTime < eventTime)
          {
            auto data = repeatIt->value();
            if(!data->m_isCancelled)
            {
              m_dispatcher(data->m_callback);
              if(!data->m_isCancelled)
              {
                data->m_handle =
                    m_repeatQueue.add(repeatTime + data->m_period, data);
              }
            }

            repeatIt++;
          }
          else
          {
            m_eventCount--;
            m_dispatcher(m_eventIt->value());
            m_eventIt++;
          }
        }

        // We've eaten one of the queues.
        while(repeatIt != pendingRepeats.end())
        {
          auto repeatTime = repeatIt->time();
          auto data       = repeatIt->value();
          if(!data->m_isCancelled)
          {
            m_dispatcher(data->m_callback);
            if(!data->m_isCancelled)
            {
              data->m_handle =
                  m_repeatQueue.add(repeatTime + data->m_period, data);
            }
          }

          repeatIt++;
        }

        while(m_eventIt != m_events.end())
        {
          m_eventCount--;
          m_dispatcher(m_eventIt->value());
          m_eventIt++;
        }

        pendingRepeats.clear();
        m_events.clear();
      }
    }

    void
    Scheduler::yield()
    {
      if(m_running.load(std::memory_order_relaxed))
      {
        if(std::this_thread::get_id() != m_thread.get_id())
        {
          size_t iterations = m_iterationCount.load(std::memory_order_relaxed);

          while(iterations == m_iterationCount.load(std::memory_order_relaxed)
                && m_running.load(std::memory_order_relaxed))
          {
            m_condition.Signal();
            std::this_thread::yield();
          }
        }
      }
    }

    Scheduler::Scheduler(EventDispatcher dispatcher, Clock clock)
        : m_clock(std::move(clock))
        , m_dispatcher(std::move(dispatcher))
        , m_running(false)
        , m_iterationCount(0)
        , m_eventIt()
        , m_repeatCount(0)
        , m_eventCount(0)
    {
    }

    Scheduler::~Scheduler()
    {
      stop();
    }

    bool
    Scheduler::start()
    {
      util::Lock threadLock(&m_threadMutex);
      util::Lock lock(&m_mutex);

      if(m_running.load(std::memory_order_relaxed))
      {
        return true;
      }

      m_thread  = std::thread(&Scheduler::dispatch, this);
      m_running = true;
      return true;
    }

    void
    Scheduler::stop()
    {
      util::Lock threadLock(&m_threadMutex);

      // Can't join holding the lock. <_<
      {
        util::Lock lock(&m_mutex);
        if(!m_running.load(std::memory_order_relaxed))
        {
          return;
        }

        m_running = false;
        m_condition.Signal();
      }

      m_thread.join();
    }

    Scheduler::Handle
    Scheduler::schedule(absl::Time time,
                        const std::function< void() >& callback,
                        const EventKey& key)
    {
      Handle handle;

      {
        util::Lock lock(&m_mutex);
        bool isAtHead = false;
        handle        = m_eventQueue.add(time, callback, key, &isAtHead);

        if(handle == -1)
        {
          return INVALID_HANDLE;
        }
        m_eventCount++;

        // If we have an event at the top of the queue, wake the dispatcher.
        if(isAtHead)
        {
          m_condition.Signal();
        }
      }

      return handle;
    }

    bool
    Scheduler::reschedule(Handle handle, absl::Time time, bool wait)
    {
      bool result = false;
      {
        util::Lock lock(&m_mutex);
        bool isAtHead = false;
        result        = m_eventQueue.update(handle, time, &isAtHead);

        if(isAtHead)
        {
          m_condition.Signal();
        }
      }

      if(result && wait)
      {
        yield();
      }

      return result;
    }

    bool
    Scheduler::reschedule(Handle handle, const EventKey& key, absl::Time time,
                          bool wait)
    {
      bool result = false;
      {
        util::Lock lock(&m_mutex);
        bool isAtHead = false;
        result        = m_eventQueue.update(handle, key, time, &isAtHead);

        if(isAtHead)
        {
          m_condition.Signal();
        }
      }

      if(result && wait)
      {
        yield();
      }

      return result;
    }

    bool
    Scheduler::cancel(Handle handle, const EventKey& key, bool wait)
    {
      if(m_eventQueue.remove(handle, key))
      {
        m_eventCount--;
        return true;
      }

      // Optimise for the dispatcher thread cancelling a pending event.
      // On the dispatch thread, so we don't have to lock.
      if(std::this_thread::get_id() == m_thread.get_id())
      {
        for(auto it = m_events.begin() + m_eventCount; it != m_events.end();
            ++it)
        {
          if(it->handle() == handle && it->key() == key)
          {
            m_eventCount--;
            m_events.erase(it);
            return true;
          }
        }

        // We didn't find it.
        return false;
      }

      if(handle != INVALID_HANDLE && wait)
      {
        yield();
      }

      return false;
    }

    void
    Scheduler::cancelAll(bool wait)
    {
      std::vector< EventItem > events;
      m_eventQueue.removeAll(&events);

      m_eventCount -= events.size();

      if(wait)
      {
        yield();
      }
    }

    Scheduler::Handle
    Scheduler::scheduleRepeat(absl::Duration interval,
                              const std::function< void() >& callback,
                              absl::Time startTime)
    {
      // Assert that we're not giving an empty duration
      assert(interval != absl::Duration());

      if(startTime == absl::Time())
      {
        startTime = interval + m_clock();
      }

      auto repeatData = std::make_shared< RepeatData >(callback, interval);

      {
        util::Lock l(&m_mutex);
        bool isAtHead = false;
        repeatData->m_handle =
            m_repeatQueue.add(startTime, repeatData, &isAtHead);

        if(repeatData->m_handle == -1)
        {
          return INVALID_HANDLE;
        }

        m_repeatCount++;

        if(isAtHead)
        {
          m_condition.Signal();
        }
      }

      return m_repeats.add(repeatData);
    }

    bool
    Scheduler::cancelRepeat(Handle handle, bool wait)
    {
      RepeatDataPtr data;

      if(!m_repeats.remove(handle, &data))
      {
        return false;
      }

      m_repeatCount--;

      if(!m_repeatQueue.remove(data->m_handle))
      {
        data->m_isCancelled = true;

        if(wait)
        {
          yield();
        }
      }

      return true;
    }

    void
    Scheduler::cancelAllRepeats(bool wait)
    {
      std::vector< RepeatDataPtr > repeats;
      m_repeats.removeAll(&repeats);

      m_repeatCount -= m_repeats.size();

      for(auto& repeat : repeats)
      {
        repeat->m_isCancelled = true;
      }

      // if we fail to remove something, we *may* have a pending repeat event in
      // the dispatcher
      bool somethingFailed = false;
      for(auto& repeat : repeats)
      {
        if(!m_repeatQueue.remove(repeat->m_handle))
        {
          somethingFailed = true;
        }
      }

      if(wait && somethingFailed)
      {
        yield();
      }
    }
  }  // namespace thread
}  // namespace llarp
