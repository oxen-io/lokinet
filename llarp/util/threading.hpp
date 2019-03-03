#ifndef LLARP_THREADING_HPP
#define LLARP_THREADING_HPP

#include <absl/synchronization/barrier.h>
#include <absl/synchronization/mutex.h>
// We only support posix threads:
// MSYS2 has a full native C++11 toolset, and a suitable
// cross-compilation system can be assembled on Linux and UNIX
// Last time i checked, Red Hat has this problem (no libpthread)
// Not sure about the other distros generally -rick
#include <condition_variable>
#include <thread>
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
      NullLock(__attribute__((unused)) NullMutex* mtx)
      {
      }
    };

    using Mutex     = absl::Mutex;
    using Lock      = absl::MutexLock;
    using Condition = absl::CondVar;

    class Semaphore
    {
     private:
      Mutex m_mutex;
      Condition m_cv;
      size_t m_count;

     public:
      Semaphore(size_t count) : m_count(count)
      {
      }

      void
      notify()
      {
        Lock lock(&m_mutex);
        m_count++;

        m_cv.Signal();
      }

      void
      wait()
      {
        Lock lock(&m_mutex);
        while(this->m_count == 0)
        {
          m_cv.Wait(&m_mutex);
        }

        m_count--;
      }

      bool
      waitFor(absl::Duration timeout)
      {
        Lock lock(&m_mutex);

        while(this->m_count == 0)
        {
          if(m_cv.WaitWithTimeout(&m_mutex, timeout))
          {
            return false;
          }
        }

        m_count--;
        return true;
      }
    };

    using Barrier = absl::Barrier;

  }  // namespace util
}  // namespace llarp

#endif
