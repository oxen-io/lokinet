#ifndef LLARP_THREADING_HPP
#define LLARP_THREADING_HPP
#include <mutex>
#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
#if defined(RPI)
#error this should not be set
#endif
#define _MINGW32_NO_THREADS
#include <llarp/win32/threads/mingw.condition_variable.h>
#include <llarp/win32/threads/mingw.mutex.h>
#include <llarp/win32/threads/mingw.thread.h>
#else
#include <condition_variable>
#include <thread>
#endif
#include <future>
#include <memory>
#include <cassert>

namespace llarp
{
  namespace util
  {
    /// a mutex that does nothing
    struct NullMutex
    {
    };

    /// a lock that does nothing
    struct NullLock
    {
      NullLock(__attribute__((unused)) NullMutex& mtx)
      {
      }
    };

    using mtx_t  = std::mutex;
    using lock_t = std::unique_lock< std::mutex >;
    using cond_t = std::condition_variable;

    struct Mutex
    {
      mtx_t impl;
    };

    /// aqcuire a lock on a mutex
    struct Lock
    {
      Lock(Mutex& mtx) : impl(mtx.impl)
      {
      }
      lock_t impl;
    };

    struct Condition
    {
      cond_t impl;
      void
      NotifyAll()
      {
        impl.notify_all();
      }

      void
      NotifyOne()
      {
        impl.notify_one();
      }

      void
      Wait(Lock& lock)
      {
        impl.wait(lock.impl);
      }

      template < typename Interval >
      void
      WaitFor(Lock& lock, Interval i)
      {
        impl.wait_for(lock.impl, i);
      }

      template < typename Pred >
      void
      WaitUntil(Lock& lock, Pred p)
      {
        impl.wait(lock.impl, p);
      }
    };

    class Semaphore
    {
     private:
      std::mutex m_mutex;
      std::condition_variable m_cv;
      size_t m_count;

     public:
      Semaphore(size_t count) : m_count(count)
      {
      }

      void
      notify()
      {
        std::unique_lock< std::mutex > lock(m_mutex);
        m_count++;

        m_cv.notify_one();
      }

      void
      wait()
      {
        std::unique_lock< std::mutex > lock(m_mutex);
        m_cv.wait(lock, [this]() { return this->m_count > 0; });

        m_count--;
      }

      template < typename Rep, typename Period >
      bool
      waitFor(const std::chrono::duration< Rep, Period >& period)
      {
        std::unique_lock< std::mutex > lock(m_mutex);

        if(m_cv.wait_for(lock, period, [this]() { return this->m_count > 0; }))
        {
          m_count--;
          return true;
        }

        return false;
      }
    };

    class Barrier
    {
     private:
      std::mutex mutex;
      std::condition_variable cv;

      const size_t numThreads;
      size_t numThreadsWaiting;  // number of threads to be woken
      size_t sigCount;    // number of times the barrier has been signalled
      size_t numPending;  // number of threads that have been signalled, but
                          // haven't woken.

     public:
      Barrier(size_t threadCount)
          : numThreads(threadCount)
          , numThreadsWaiting(0)
          , sigCount(0)
          , numPending(0)
      {
      }

      ~Barrier()
      {
        for(;;)
        {
          {
            std::unique_lock< std::mutex > lock(mutex);
            if(numPending == 0)
            {
              break;
            }
          }

          std::this_thread::yield();
        }

        assert(numThreadsWaiting == 0);
      }

      void
      wait()
      {
        std::unique_lock< std::mutex > lock(mutex);
        size_t signalCount = sigCount;

        if(++numThreadsWaiting == numThreads)
        {
          ++sigCount;
          numPending += numThreads - 1;
          numThreadsWaiting = 0;
          cv.notify_all();
        }
        else
        {
          cv.wait(lock, [this, signalCount]() {
            return this->sigCount != signalCount;
          });

          --numPending;
        }
      }
    };

  }  // namespace util
}  // namespace llarp

#endif
