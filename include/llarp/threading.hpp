#ifndef LLARP_THREADING_HPP
#define LLARP_THREADING_HPP
#include <mutex>
#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
#include <llarp/win32/threads/mingw.condition_variable.h>
#include <llarp/win32/threads/mingw.mutex.h>
#include <llarp/win32/threads/mingw.thread.h>
#else
#include <condition_variable>
#include <thread>
#endif

#include <memory>

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
      NullLock(NullMutex& mtx)
      {
      }
    };

    /// a condition variable that does nothing
    struct NullCondition
    {
      void
      wait(NullLock& l)
      {
      }

      void
      notify_one()
      {
      }

      void
      notify_all()
      {
      }

      template < typename Interval >
      void
      wait_for(NullLock& l, Interval i)
      {
        std::this_thread::sleep_for(i);
      }
    };

#ifdef SHADOW_TESTNET
    typedef NullMutex mtx_t;
    typedef NullLock lock_t;
    typedef NullCondition cond_t;
#else
    typedef std::mutex mtx_t;
    typedef std::unique_lock< std::mutex > lock_t;
    typedef std::condition_variable cond_t;
#endif

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

  }  // namespace util
}  // namespace llarp

#endif