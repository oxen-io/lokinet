#pragma once

#include "annotations.hpp"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>

#if defined(WIN32) && !defined(__GNUC__)
#include <process.h>

using pid_t = int;
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef TRACY_ENABLE
#include <Tracy.hpp>

#define DECLARE_LOCK(type, var, ...) TracyLockable(type, var)
#else
#define DECLARE_LOCK(type, var, ...) type var __VA_ARGS__
#endif

namespace llarp
{
  namespace util
  {
    /// a mutex that does nothing
    ///
    /// this exists to convert mutexes that were initially in use (but may no
    /// longer be necessary) into no-op placeholders (except in debug mode
    /// where they complain loudly when they are actually accessed across
    /// different threads; see below).
    ///
    /// the idea is to "turn off" the mutexes and see where they are actually
    /// needed.
    struct CAPABILITY("mutex") NullMutex
    {
#ifdef LOKINET_DEBUG
      /// in debug mode, we implement lock() to enforce that any lock is only
      /// used from a single thread. the point of this is to identify locks that
      /// are actually needed by dying a painful death when used across threads
      mutable std::optional<std::thread::id> m_id;
      void
      lock() const
      {
        if (!m_id)
        {
          m_id = std::this_thread::get_id();
        }
        else if (*m_id != std::this_thread::get_id())
        {
          std::cerr << "NullMutex " << this << " was used across threads: locked by "
                    << std::this_thread::get_id() << " and was previously locked by " << *m_id
                    << "\n";
          // if you're encountering this abort() call, you may have discovered a
          // case where a NullMutex should be reverted to a "real mutex"
          std::abort();
        }
      }
#else
      void
      lock() const
      {}
#endif
      // Does nothing; once locked the mutex belongs to that thread forever
      void
      unlock() const
      {}
    };

    /// a lock that does nothing
    struct SCOPED_CAPABILITY NullLock
    {
      NullLock(NullMutex& mtx) ACQUIRE(mtx)
      {
        mtx.lock();
      }

      ~NullLock() RELEASE()
      {
        (void)this;  // trick clang-tidy
      }
    };

    /// Default mutex type, supporting shared and exclusive locks.
    using Mutex = std::shared_timed_mutex;

    /// Basic RAII lock type for the default mutex type.
    using Lock = std::lock_guard<Mutex>;

    /// Obtains multiple unique locks simultaneously and atomically.  Returns a
    /// tuple of all the held locks.
    template <typename... Mutex>
    [[nodiscard]] auto
    unique_locks(Mutex&... lockables)
    {
      std::lock(lockables...);
      return std::make_tuple(std::unique_lock{lockables, std::adopt_lock}...);
    }

    class Semaphore
    {
     private:
      std::mutex m_mutex;  // protects m_count
      size_t m_count GUARDED_BY(m_mutex);
      std::condition_variable m_cv;

     public:
      Semaphore(size_t count) : m_count(count)
      {}

      void
      notify() EXCLUDES(m_mutex)
      {
        {
          std::lock_guard<std::mutex> lock(m_mutex);
          m_count++;
        }
        m_cv.notify_one();
      }

      void
      wait() EXCLUDES(m_mutex)
      {
        std::unique_lock lock{m_mutex};
        m_cv.wait(lock, [this] { return m_count > 0; });
        m_count--;
      }

      bool
      waitFor(std::chrono::microseconds timeout) EXCLUDES(m_mutex)
      {
        std::unique_lock lock{m_mutex};
        if (!m_cv.wait_for(lock, timeout, [this] { return m_count > 0; }))
          return false;

        m_count--;
        return true;
      }
    };

    void
    SetThreadName(const std::string& name);

    inline pid_t
    GetPid()
    {
#ifdef WIN32
      return _getpid();
#else
      return ::getpid();
#endif
    }

    // type for detecting contention on a resource
    struct ContentionKiller
    {
      template <typename F>
      void
      TryAccess(F visit) const
#if defined(LOKINET_DEBUG)
          EXCLUDES(_access)
#endif
      {
#if defined(LOKINET_DEBUG)
        NullLock lock(_access);
#endif
        visit();
      }
#if defined(LOKINET_DEBUG)
     private:
      mutable NullMutex _access;
#endif
    };
  }  // namespace util
}  // namespace llarp
