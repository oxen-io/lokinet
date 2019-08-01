#ifndef LLARP_THREADING_HPP
#define LLARP_THREADING_HPP

#include <absl/synchronization/barrier.h>
#include <absl/synchronization/mutex.h>
#include <absl/time/time.h>

#if defined(WIN32) && !defined(__GNUC__)
#include <process.h>
using pid_t = int;
#else
#include <sys/types.h>
#include <unistd.h>
#endif

namespace llarp
{
  namespace util
  {
    /// a mutex that does nothing
    struct LOCKABLE NullMutex
    {
    };

    /// a lock that does nothing
    struct SCOPED_LOCKABLE NullLock
    {
      NullLock(ABSL_ATTRIBUTE_UNUSED const NullMutex* mtx)
          EXCLUSIVE_LOCK_FUNCTION(mtx)
      {
      }

      ~NullLock() UNLOCK_FUNCTION()
      {
      }
    };

    using Mutex          = absl::Mutex;
    using Lock           = absl::MutexLock;
    using ReleasableLock = absl::ReleasableMutexLock;
    using Condition      = absl::CondVar;

    class Semaphore
    {
     private:
      Mutex m_mutex;  // protects m_count
      size_t m_count GUARDED_BY(m_mutex);

      bool
      ready() const SHARED_LOCKS_REQUIRED(m_mutex)
      {
        return m_count > 0;
      }

     public:
      Semaphore(size_t count) : m_count(count)
      {
      }

      void
      notify() LOCKS_EXCLUDED(m_mutex)
      {
        Lock lock(&m_mutex);
        m_count++;
      }

      void
      wait() LOCKS_EXCLUDED(m_mutex)
      {
        Lock lock(&m_mutex);
        m_mutex.Await(absl::Condition(this, &Semaphore::ready));

        m_count--;
      }

      bool
      waitFor(absl::Duration timeout) LOCKS_EXCLUDED(m_mutex)
      {
        Lock lock(&m_mutex);

        if(!m_mutex.AwaitWithTimeout(absl::Condition(this, &Semaphore::ready),
                                     timeout))
        {
          return false;
        }

        m_count--;
        return true;
      }
    };

    using Barrier = absl::Barrier;

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
  }  // namespace util
}  // namespace llarp

#endif
