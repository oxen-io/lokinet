/**
 * @file condition_variable.h
 * @brief std::condition_variable implementation for MinGW
 *
 * (c) 2013-2016 by Mega Limited, Auckland, New Zealand
 * @author Alexander Vassilev
 *
 * @copyright Simplified (2-clause) BSD License.
 * You should have received a copy of the license along with this
 * program.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * @note
 * This file may become part of the mingw-w64 runtime package. If/when this
 * happens, the appropriate license will be added, i.e. this code will become
 * dual-licensed, and the current BSD 2-clause license will stay.
 */

#ifndef MINGW_CONDITIONAL_VARIABLE_H
#define MINGW_CONDITIONAL_VARIABLE_H

#if !defined(__cplusplus) || (__cplusplus < 201103L)
#error A C++11 compiler is required!
#endif
//  Use the standard classes for std::, if available.
#include <condition_variable>

#include <windows.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <system_error>
#include "mingw.mutex.h"
#include "mingw.shared_mutex.h"

namespace mingw_stdthread
{
#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
  enum class cv_status
  {
    no_timeout,
    timeout
  };
#else
  using std::cv_status;
#endif
  namespace xp
  {
//    Include the XP-compatible condition_variable classes only if actually
//  compiling for XP. The XP-compatible classes are slower than the newer
//  versions, and depend on features not compatible with Windows Phone 8.
#if(WINVER < _WIN32_WINNT_VISTA)
    class condition_variable_any
    {
     protected:
      recursive_mutex mMutex;
      std::atomic< int > mNumWaiters;
      HANDLE mSemaphore;
      HANDLE mWakeEvent;

     public:
      typedef HANDLE native_handle_type;
      native_handle_type
      native_handle()
      {
        return mSemaphore;
      }
      condition_variable_any(const condition_variable_any&) = delete;
      condition_variable_any&
      operator=(const condition_variable_any&) = delete;
      condition_variable_any()
          : mMutex()
          , mNumWaiters(0)
          , mSemaphore(CreateSemaphore(NULL, 0, 0xFFFF, NULL))
          , mWakeEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
      {
      }
      ~condition_variable_any()
      {
        CloseHandle(mWakeEvent);
        CloseHandle(mSemaphore);
      }

     protected:
      template < class M >
      bool
      wait_impl(M& lock, DWORD timeout)
      {
        {
          lock_guard< recursive_mutex > guard(mMutex);
          mNumWaiters++;
        }
        lock.unlock();
        DWORD ret = WaitForSingleObject(mSemaphore, timeout);

        mNumWaiters--;
        SetEvent(mWakeEvent);
        lock.lock();
        if(ret == WAIT_OBJECT_0)
          return true;
        else if(ret == WAIT_TIMEOUT)
          return false;
        // 2 possible cases:
        // 1)The point in notify_all() where we determine the count to
        // increment the semaphore with has not been reached yet:
        // we just need to decrement mNumWaiters, but setting the event does not
        // hurt
        //
        // 2)Semaphore has just been released with mNumWaiters just before
        // we decremented it. This means that the semaphore count
        // after all waiters finish won't be 0 - because not all waiters
        // woke up by acquiring the semaphore - we woke up by a timeout.
        // The notify_all() must handle this grafecully
        //
        else
        {
          using namespace std;
          throw system_error(make_error_code(errc::protocol_error));
        }
      }

     public:
      template < class M >
      void
      wait(M& lock)
      {
        wait_impl(lock, INFINITE);
      }
      template < class M, class Predicate >
      void
      wait(M& lock, Predicate pred)
      {
        while(!pred())
        {
          wait(lock);
        };
      }

      void
      notify_all() noexcept
      {
        lock_guard< recursive_mutex > lock(
            mMutex);  // block any further wait requests until all current
                      // waiters are unblocked
        if(mNumWaiters.load() <= 0)
          return;

        ReleaseSemaphore(mSemaphore, mNumWaiters, NULL);
        while(mNumWaiters > 0)
        {
          auto ret = WaitForSingleObject(mWakeEvent, 1000);
          if(ret == WAIT_FAILED || ret == WAIT_ABANDONED)
            std::terminate();
        }
        assert(mNumWaiters == 0);
        // in case some of the waiters timed out just after we released the
        // semaphore by mNumWaiters, it won't be zero now, because not all
        // waiters woke up by acquiring the semaphore. So we must zero the
        // semaphore before we accept waiters for the next event See _wait_impl
        // for details
        while(WaitForSingleObject(mSemaphore, 0) == WAIT_OBJECT_0)
          ;
      }
      void
      notify_one() noexcept
      {
        lock_guard< recursive_mutex > lock(mMutex);
        int targetWaiters = mNumWaiters.load() - 1;
        if(targetWaiters <= -1)
          return;
        ReleaseSemaphore(mSemaphore, 1, NULL);
        while(mNumWaiters > targetWaiters)
        {
          auto ret = WaitForSingleObject(mWakeEvent, 1000);
          if(ret == WAIT_FAILED || ret == WAIT_ABANDONED)
            std::terminate();
        }
        assert(mNumWaiters == targetWaiters);
      }
      template < class M, class Rep, class Period >
      cv_status
      wait_for(M& lock, const std::chrono::duration< Rep, Period >& rel_time)
      {
        using namespace std::chrono;
        long long timeout = duration_cast< milliseconds >(rel_time).count();
        if(timeout < 0)
          timeout = 0;
        bool ret = wait_impl(lock, (DWORD)timeout);
        return ret ? cv_status::no_timeout : cv_status::timeout;
      }

      template < class M, class Rep, class Period, class Predicate >
      bool
      wait_for(M& lock, const std::chrono::duration< Rep, Period >& rel_time,
               Predicate pred)
      {
        return wait_until(lock, std::chrono::steady_clock::now() + rel_time,
                          pred);
      }
      template < class M, class Clock, class Duration >
      cv_status
      wait_until(M& lock,
                 const std::chrono::time_point< Clock, Duration >& abs_time)
      {
        return wait_for(lock, abs_time - Clock::now());
      }
      template < class M, class Clock, class Duration, class Predicate >
      bool
      wait_until(M& lock,
                 const std::chrono::time_point< Clock, Duration >& abs_time,
                 Predicate pred)
      {
        while(!pred())
        {
          if(wait_until(lock, abs_time) == cv_status::timeout)
          {
            return pred();
          }
        }
        return true;
      }
    };
    class condition_variable : protected condition_variable_any
    {
     protected:
      typedef condition_variable_any base;

     public:
      using base::base;
      using base::native_handle;
      using base::native_handle_type;
      using base::notify_all;
      using base::notify_one;
      void
      wait(unique_lock< mutex >& lock)
      {
        base::wait(lock);
      }
      template < class Predicate >
      void
      wait(unique_lock< mutex >& lock, Predicate pred)
      {
        base::wait(lock, pred);
      }
      template < class Rep, class Period >
      cv_status
      wait_for(unique_lock< mutex >& lock,
               const std::chrono::duration< Rep, Period >& rel_time)
      {
        return base::wait_for(lock, rel_time);
      }
      template < class Rep, class Period, class Predicate >
      bool
      wait_for(unique_lock< mutex >& lock,
               const std::chrono::duration< Rep, Period >& rel_time,
               Predicate pred)
      {
        return base::wait_for(lock, rel_time, pred);
      }
      template < class Clock, class Duration >
      cv_status
      wait_until(unique_lock< mutex >& lock,
                 const std::chrono::time_point< Clock, Duration >& abs_time)
      {
        return base::wait_until(lock, abs_time);
      }
      template < class Clock, class Duration, class Predicate >
      bool
      wait_until(unique_lock< mutex >& lock,
                 const std::chrono::time_point< Clock, Duration >& abs_time,
                 Predicate pred)
      {
        return base::wait_until(lock, abs_time, pred);
      }
    };
#endif  //  Compiling for XP
  }     // namespace xp

#if(WINVER >= _WIN32_WINNT_VISTA)
  namespace vista
  {
    //  If compiling for Vista or higher, use the native condition variable.
    class condition_variable
    {
     protected:
      CONDITION_VARIABLE cvariable_;

#if STDMUTEX_RECURSION_CHECKS
      template < typename MTX >
      inline static void
      before_wait(MTX* pmutex)
      {
        pmutex->mOwnerThread.checkSetOwnerBeforeUnlock();
      }
      template < typename MTX >
      inline static void
      after_wait(MTX* pmutex)
      {
        pmutex->mOwnerThread.setOwnerAfterLock(GetCurrentThreadId());
      }
#else
      inline static void
      before_wait(void*)
      {
      }
      inline static void
      after_wait(void*)
      {
      }
#endif

      bool
      wait_impl(unique_lock< xp::mutex >& lock, DWORD time)
      {
        static_assert(std::is_same< typename xp::mutex::native_handle_type,
                                    PCRITICAL_SECTION >::value,
                      "Native Win32 condition variable requires std::mutex to \
use native Win32 critical section objects.");
        xp::mutex* pmutex = lock.release();
        before_wait(pmutex);
        BOOL success = SleepConditionVariableCS(&cvariable_,
                                                pmutex->native_handle(), time);
        after_wait(pmutex);
        lock = unique_lock< xp::mutex >(*pmutex, adopt_lock);
        return success;
      }

      bool
      wait_unique(windows7::mutex* pmutex, DWORD time)
      {
        before_wait(pmutex);
        BOOL success = SleepConditionVariableSRW(
            native_handle(), pmutex->native_handle(), time, 0);
        after_wait(pmutex);
        return success;
      }
      bool
      wait_impl(unique_lock< windows7::mutex >& lock, DWORD time)
      {
        windows7::mutex* pmutex = lock.release();
        bool success            = wait_unique(pmutex, time);
        lock = unique_lock< windows7::mutex >(*pmutex, adopt_lock);
        return success;
      }

     public:
      typedef PCONDITION_VARIABLE native_handle_type;
      native_handle_type
      native_handle(void)
      {
        return &cvariable_;
      }

      condition_variable(void) : cvariable_()
      {
        InitializeConditionVariable(&cvariable_);
      }

      ~condition_variable(void) = default;

      condition_variable(const condition_variable&) = delete;
      condition_variable&
      operator=(const condition_variable&) = delete;

      void
      notify_one(void) noexcept
      {
        WakeConditionVariable(&cvariable_);
      }

      void
      notify_all(void) noexcept
      {
        WakeAllConditionVariable(&cvariable_);
      }

      void
      wait(unique_lock< mutex >& lock)
      {
        wait_impl(lock, INFINITE);
      }

      template < class Predicate >
      void
      wait(unique_lock< mutex >& lock, Predicate pred)
      {
        while(!pred())
          wait(lock);
      }

      template < class Rep, class Period >
      cv_status
      wait_for(unique_lock< mutex >& lock,
               const std::chrono::duration< Rep, Period >& rel_time)
      {
        using namespace std::chrono;
        auto time = duration_cast< milliseconds >(rel_time).count();
        if(time < 0)
          time = 0;
        bool result = wait_impl(lock, static_cast< DWORD >(time));
        return result ? cv_status::no_timeout : cv_status::timeout;
      }

      template < class Rep, class Period, class Predicate >
      bool
      wait_for(unique_lock< mutex >& lock,
               const std::chrono::duration< Rep, Period >& rel_time,
               Predicate pred)
      {
        return wait_until(lock, std::chrono::steady_clock::now() + rel_time,
                          std::move(pred));
      }
      template < class Clock, class Duration >
      cv_status
      wait_until(unique_lock< mutex >& lock,
                 const std::chrono::time_point< Clock, Duration >& abs_time)
      {
        return wait_for(lock, abs_time - Clock::now());
      }
      template < class Clock, class Duration, class Predicate >
      bool
      wait_until(unique_lock< mutex >& lock,
                 const std::chrono::time_point< Clock, Duration >& abs_time,
                 Predicate pred)
      {
        while(!pred())
        {
          if(wait_until(lock, abs_time) == cv_status::timeout)
          {
            return pred();
          }
        }
        return true;
      }
    };

    class condition_variable_any : protected condition_variable
    {
     protected:
      typedef condition_variable base;
      typedef windows7::shared_mutex native_shared_mutex;

      //    When available, the SRW-based mutexes should be faster than the
      //  CriticalSection-based mutexes. Only try_lock will be unavailable in
      //  Vista, and try_lock is not used by condition_variable_any.
      windows7::mutex internal_mutex_;

      template < class L >
      bool
      wait_impl(L& lock, DWORD time)
      {
        unique_lock< decltype(internal_mutex_) > internal_lock(internal_mutex_);
        lock.unlock();
        bool success = base::wait_impl(internal_lock, time);
        lock.lock();
        return success;
      }
      //    If the lock happens to be called on a native Windows mutex, skip any
      //    extra
      //  contention.
      inline bool
      wait_impl(unique_lock< mutex >& lock, DWORD time)
      {
        return base::wait_impl(lock, time);
      }
      //    Some shared_mutex functionality is available even in Vista, but it's
      //    not
      //  until Windows 7 that a full implementation is natively possible. The
      //  class itself is defined, with missing features, at the Vista feature
      //  level.
      static_assert(CONDITION_VARIABLE_LOCKMODE_SHARED != 0,
                    "The flag \
CONDITION_VARIABLE_LOCKMODE_SHARED is not defined as expected. The value for \
exclusive mode is unknown (not specified by Microsoft Dev Center), but assumed \
to be 0. There is a conflict with CONDITION_VARIABLE_LOCKMODE_SHARED.");
      //#if (WINVER >= _WIN32_WINNT_VISTA)
      bool
      wait_impl(unique_lock< native_shared_mutex >& lock, DWORD time)
      {
        native_shared_mutex* pmutex = lock.release();
        bool success                = wait_unique(pmutex, time);
        lock = unique_lock< native_shared_mutex >(*pmutex, adopt_lock);
        return success;
      }
      bool
      wait_impl(shared_lock< native_shared_mutex >& lock, DWORD time)
      {
        native_shared_mutex* pmutex = lock.release();
        BOOL success                = SleepConditionVariableSRW(
            base::native_handle(), pmutex->native_handle(), time,
            CONDITION_VARIABLE_LOCKMODE_SHARED);
        lock = shared_lock< native_shared_mutex >(*pmutex, adopt_lock);
        return success;
      }
      //#endif
     public:
      typedef typename base::native_handle_type native_handle_type;
      using base::native_handle;

      condition_variable_any(void) : base(), internal_mutex_()
      {
      }

      ~condition_variable_any(void) = default;

      using base::notify_all;
      using base::notify_one;

      template < class L >
      void
      wait(L& lock)
      {
        wait_impl(lock, INFINITE);
      }

      template < class L, class Predicate >
      void
      wait(L& lock, Predicate pred)
      {
        while(!pred())
          wait(lock);
      }

      template < class L, class Rep, class Period >
      cv_status
      wait_for(L& lock, const std::chrono::duration< Rep, Period >& period)
      {
        using namespace std::chrono;
        auto time = duration_cast< milliseconds >(period).count();
        if(time < 0)
          time = 0;
        bool result = wait_impl(lock, static_cast< DWORD >(time));
        return result ? cv_status::no_timeout : cv_status::timeout;
      }

      template < class L, class Rep, class Period, class Predicate >
      bool
      wait_for(L& lock, const std::chrono::duration< Rep, Period >& period,
               Predicate pred)
      {
        return wait_until(lock, std::chrono::steady_clock::now() + period,
                          std::move(pred));
      }
      template < class L, class Clock, class Duration >
      cv_status
      wait_until(L& lock,
                 const std::chrono::time_point< Clock, Duration >& abs_time)
      {
        return wait_for(lock, abs_time - Clock::now());
      }
      template < class L, class Clock, class Duration, class Predicate >
      bool
      wait_until(L& lock,
                 const std::chrono::time_point< Clock, Duration >& abs_time,
                 Predicate pred)
      {
        while(!pred())
        {
          if(wait_until(lock, abs_time) == cv_status::timeout)
          {
            return pred();
          }
        }
        return true;
      }
    };
  }  //  Namespace vista
#endif
#if WINVER < 0x0600
  using xp::condition_variable;
  using xp::condition_variable_any;
#else
  using vista::condition_variable;
  using vista::condition_variable_any;
#endif
}  //  Namespace mingw_stdthread

//  Push objects into std, but only if they are not already there.
namespace std
{
//    Because of quirks of the compiler, the common "using namespace std;"
//  directive would flatten the namespaces and introduce ambiguity where there
//  was none. Direct specification (std::), however, would be unaffected.
//    Take the safe option, and include only in the presence of MinGW's win32
//  implementation.
#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
  using mingw_stdthread::condition_variable;
  using mingw_stdthread::condition_variable_any;
  using mingw_stdthread::cv_status;
#elif !defined(MINGW_STDTHREAD_REDUNDANCY_WARNING)  //  Skip repetition
#define MINGW_STDTHREAD_REDUNDANCY_WARNING
#pragma message \
    "This version of MinGW seems to include a win32 port of\
 pthreads, and probably already has C++11 std threading classes implemented,\
 based on pthreads. These classes, found in namespace std, are not overridden\
 by the mingw-std-thread library. If you would still like to use this\
 implementation (as it is more lightweight), use the classes provided in\
 namespace mingw_stdthread."
#endif
}  // namespace std
#endif  // MINGW_CONDITIONAL_VARIABLE_H
