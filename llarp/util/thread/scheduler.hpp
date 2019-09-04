#ifndef LLARP_SCHEDULER_HPP
#define LLARP_SCHEDULER_HPP

#include <util/meta/object.hpp>
#include <util/thread/timerqueue.hpp>

#include <absl/time/time.h>
#include <atomic>
#include <functional>
#include <thread>
#include <utility>
#include <vector>

namespace llarp
{
  namespace thread
  {
    /// This is a general purpose event scheduler, supporting both one-off and
    /// repeated events.
    ///
    /// Notes:
    /// - Events should not be started before their begin time
    /// - Events may start an arbitrary amount of time after they are scheduled,
    ///   if there is a previous long running event.
    class Scheduler
    {
     public:
      using Callback = std::function< void() >;
      using Handle   = int;
      static const Handle INVALID_HANDLE;

      // Define our own clock so we can test easier
      using Clock = std::function< absl::Time() >;

     private:
      /// struct for repeated events
      struct RepeatData
      {
        Callback m_callback;
        absl::Duration m_period;
        std::atomic_bool m_isCancelled;
        Handle m_handle;

        RepeatData(Callback callback, absl::Duration period)
            : m_callback(std::move(callback))
            , m_period(period)
            , m_isCancelled(false)
            , m_handle(0)
        {
        }
      };

      using RepeatDataPtr = std::shared_ptr< RepeatData >;
      using RepeatQueue   = TimerQueue< RepeatDataPtr >;
      // Just for naming purposes.
      using Event      = Callback;
      using EventQueue = TimerQueue< Event >;
      using EventItem  = TimerQueueItem< Event >;

     public:
      // Looks more horrible than it is.
      using EventDispatcher = std::function< void(const Callback&) >;

      using EventKey = EventQueue::Key;

     private:
      Clock m_clock;
      EventQueue m_eventQueue;
      RepeatQueue m_repeatQueue;
      object::Catalog< RepeatDataPtr > m_repeats;

      util::Mutex m_threadMutex ACQUIRED_BEFORE(m_mutex);  // protects running
      util::Mutex m_mutex ACQUIRED_AFTER(m_threadMutex);   // master mutex
      absl::CondVar m_condition;

      EventDispatcher m_dispatcher;
      std::thread m_thread;

      std::atomic_bool m_running;
      std::atomic_size_t m_iterationCount;

      std::vector< EventItem > m_events;
      std::vector< EventItem >::iterator m_eventIt;

      std::atomic_size_t m_repeatCount;
      std::atomic_size_t m_eventCount;

      Scheduler(const Scheduler&) = delete;
      Scheduler&
      operator=(const Scheduler&) = delete;

      friend class DispatcherImpl;
      friend class Tardis;

      /// Dispatch thread function
      void
      dispatch();

      /// Yield to the dispatch thread
      void
      yield();

     public:
      /// Return the epoch from which to create `Durations` from.
      static absl::Time
      epoch()
      {
        return absl::UnixEpoch();
      }

      static EventDispatcher
      defaultDispatcher()
      {
        return [](const Callback& callback) { callback(); };
      }

      static Clock
      defaultClock()
      {
        return &absl::Now;
      }

      Scheduler() : Scheduler(defaultDispatcher(), defaultClock())
      {
      }

      explicit Scheduler(const EventDispatcher& dispatcher)
          : Scheduler(dispatcher, defaultClock())
      {
      }

      explicit Scheduler(const Clock& clock)
          : Scheduler(defaultDispatcher(), clock)
      {
      }

      Scheduler(EventDispatcher dispatcher, Clock clock);

      ~Scheduler();

      /// Start the scheduler
      /// Note that currently this "can't fail" and return `false`. If thread
      /// spawning fails, an exception will be thrown.
      bool
      start();

      void
      stop();

      Handle
      schedule(absl::Time time, const Callback& callback,
               const EventKey& key = EventKey(nullptr));

      bool
      reschedule(Handle handle, absl::Time time, bool wait = false);
      bool
      reschedule(Handle handle, const EventKey& key, absl::Time time,
                 bool wait = false);

      bool
      cancel(Handle handle, bool wait = false)
      {
        return cancel(handle, EventKey(nullptr), wait);
      }
      bool
      cancel(Handle handle, const EventKey& key, bool wait = false);

      void
      cancelAll(bool wait = false);

      Handle
      scheduleRepeat(absl::Duration interval, const Callback& callback,
                     absl::Time startTime = absl::Time());

      bool
      cancelRepeat(Handle handle, bool wait = false);

      void
      cancelAllRepeats(bool wait = false);

      size_t
      repeatCount() const
      {
        return m_repeatCount;
      }

      size_t
      eventCount() const
      {
        return m_eventCount;
      }
    };

    class Tardis
    {
      mutable util::Mutex m_mutex;
      absl::Time m_time;
      Scheduler& m_scheduler;

     public:
      Tardis(Scheduler& scheduler) : m_time(absl::Now()), m_scheduler(scheduler)
      {
        m_scheduler.m_clock = std::bind(&Tardis::now, this);
      }

      void
      advanceTime(absl::Duration duration)
      {
        {
          absl::WriterMutexLock l(&m_mutex);
          m_time += duration;
        }

        {
          absl::WriterMutexLock l(&m_scheduler.m_mutex);
          m_scheduler.m_condition.Signal();
        }
      }

      absl::Time
      now() const
      {
        absl::ReaderMutexLock l(&m_mutex);
        return m_time;
      }
    };
  }  // namespace thread

}  // namespace llarp

#endif
