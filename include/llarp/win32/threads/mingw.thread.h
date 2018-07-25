/**
 * @file mingw.thread.h
 * @brief std::thread implementation for MinGW
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

#ifndef WIN32STDTHREAD_H
#define WIN32STDTHREAD_H

#if !defined(__cplusplus) || (__cplusplus < 201103L)
#error A C++11 compiler is required!
#endif

//  Use the standard classes for std::, if available.
#include <thread>

#include <process.h>
#include <windows.h>
#include <cerrno>
#include <chrono>
#include <functional>
#include <memory>
#include <ostream>
#include <system_error>
#include <type_traits>

#ifndef NDEBUG
#include <cstdio>
#endif

// instead of INVALID_HANDLE_VALUE _beginthreadex returns 0
#define _STD_THREAD_INVALID_HANDLE 0
namespace mingw_stdthread
{
  namespace detail
  {
//  For compatibility, implement std::invoke for C++11 and C++14
#if __cplusplus < 201703L
    template < bool PMemFunc, bool PMemData >
    struct Invoker
    {
      template < class F, class... Args >
      inline static typename std::result_of< F(Args...) >::type
      invoke(F&& f, Args&&... args)
      {
        return std::forward< F >(f)(std::forward< Args >(args)...);
      }
    };
    template < bool >
    struct InvokerHelper;

    template <>
    struct InvokerHelper< false >
    {
      template < class T1 >
      inline static auto
      get(T1&& t1) -> decltype(*std::forward< T1 >(t1))
      {
        return *std::forward< T1 >(t1);
      }

      template < class T1 >
      inline static auto
      get(const std::reference_wrapper< T1 >& t1) -> decltype(t1.get())
      {
        return t1.get();
      }
    };

    template <>
    struct InvokerHelper< true >
    {
      template < class T1 >
      inline static auto
      get(T1&& t1) -> decltype(std::forward< T1 >(t1))
      {
        return std::forward< T1 >(t1);
      }
    };

    template <>
    struct Invoker< true, false >
    {
      template < class T, class F, class T1, class... Args >
      inline static auto
      invoke(F T::*f, T1&& t1, Args&&... args) -> decltype((
          InvokerHelper< std::is_base_of< T, typename std::decay< T1 >::type >::
                             value >::get(std::forward< T1 >(t1))
          .*f)(std::forward< Args >(args)...))
      {
        return (
            InvokerHelper<
                std::is_base_of< T, typename std::decay< T1 >::type >::value >::
                get(std::forward< T1 >(t1))
            .*f)(std::forward< Args >(args)...);
      }
    };

    template <>
    struct Invoker< false, true >
    {
      template < class T, class F, class T1, class... Args >
      inline static auto
      invoke(F T::*f, T1&& t1, Args&&... args)
          -> decltype(InvokerHelper< std::is_base_of<
                          T, typename std::decay< T1 >::type >::value >::get(t1)
                      .*f)
      {
        return InvokerHelper< std::is_base_of<
                   T, typename std::decay< T1 >::type >::value >::get(t1)
            .*f;
      }
    };

    template < class F, class... Args >
    struct InvokeResult
    {
      typedef Invoker< std::is_member_function_pointer<
                           typename std::remove_reference< F >::type >::value,
                       std::is_member_object_pointer<
                           typename std::remove_reference< F >::type >::value
                           && (sizeof...(Args) == 1) >
          invoker;
      inline static auto
      invoke(F&& f, Args&&... args)
          -> decltype(invoker::invoke(std::forward< F >(f),
                                      std::forward< Args >(args)...))
      {
        return invoker::invoke(std::forward< F >(f),
                               std::forward< Args >(args)...);
      };
    };

    template < class F, class... Args >
    auto
    invoke(F&& f, Args&&... args)
        -> decltype(InvokeResult< F, Args... >::invoke(
            std::forward< F >(f), std::forward< Args >(args)...))
    {
      return InvokeResult< F, Args... >::invoke(std::forward< F >(f),
                                                std::forward< Args >(args)...);
    }
#else
    using std::invoke;
#endif

    template < int... >
    struct IntSeq
    {
    };

    template < int N, int... S >
    struct GenIntSeq : GenIntSeq< N - 1, N - 1, S... >
    {
    };

    template < int... S >
    struct GenIntSeq< 0, S... >
    {
      typedef IntSeq< S... > type;
    };

    // We can't define the Call struct in the function - the standard forbids
    // template methods in that case
    template < class Func, typename... Args >
    struct ThreadFuncCall
    {
      typedef std::tuple< Args... > Tuple;
      Func mFunc;
      Tuple mArgs;
      ThreadFuncCall(Func&& aFunc, Args&&... aArgs)
          : mFunc(std::forward< Func >(aFunc))
          , mArgs(std::forward< Args >(aArgs)...)
      {
      }
      template < int... S >
      void
      callFunc(detail::IntSeq< S... >)
      {
        detail::invoke(std::forward< Func >(mFunc),
                       std::get< S >(std::forward< Tuple >(mArgs))...);
      }
    };

  }  // namespace detail

  class thread
  {
   public:
    class id
    {
      DWORD mId;
      void
      clear()
      {
        mId = 0;
      }
      friend class thread;
      friend class std::hash< id >;

     public:
      explicit id(DWORD aId = 0) noexcept : mId(aId)
      {
      }
      friend bool
      operator==(id x, id y) noexcept
      {
        return x.mId == y.mId;
      }
      friend bool
      operator!=(id x, id y) noexcept
      {
        return x.mId != y.mId;
      }
      friend bool
      operator<(id x, id y) noexcept
      {
        return x.mId < y.mId;
      }
      friend bool
      operator<=(id x, id y) noexcept
      {
        return x.mId <= y.mId;
      }
      friend bool
      operator>(id x, id y) noexcept
      {
        return x.mId > y.mId;
      }
      friend bool
      operator>=(id x, id y) noexcept
      {
        return x.mId >= y.mId;
      }

      template < class _CharT, class _Traits >
      friend std::basic_ostream< _CharT, _Traits >&
      operator<<(std::basic_ostream< _CharT, _Traits >& __out, id __id)
      {
        if(__id.mId == 0)
        {
          return __out << "(invalid std::thread::id)";
        }
        else
        {
          return __out << __id.mId;
        }
      }
    };

   protected:
    HANDLE mHandle;
    id mThreadId;

   public:
    typedef HANDLE native_handle_type;
    id
    get_id() const noexcept
    {
      return mThreadId;
    }
    native_handle_type
    native_handle() const
    {
      return mHandle;
    }
    thread() : mHandle(_STD_THREAD_INVALID_HANDLE), mThreadId()
    {
    }

    thread(thread&& other) : mHandle(other.mHandle), mThreadId(other.mThreadId)
    {
      other.mHandle = _STD_THREAD_INVALID_HANDLE;
      other.mThreadId.clear();
    }

    thread(const thread& other) = delete;

    template < class Func, typename... Args >
    explicit thread(Func&& func, Args&&... args) : mHandle(), mThreadId()
    {
      typedef detail::ThreadFuncCall< Func, Args... > Call;
      auto call =
          new Call(std::forward< Func >(func), std::forward< Args >(args)...);
      mHandle =
          (HANDLE)_beginthreadex(NULL, 0, threadfunc< Call, Args... >,
                                 (LPVOID)call, 0, (unsigned*)&(mThreadId.mId));
      if(mHandle == _STD_THREAD_INVALID_HANDLE)
      {
        int errnum = errno;
        delete call;
        //  Note: Should only throw EINVAL, EAGAIN, EACCES
        throw std::system_error(errnum, std::generic_category());
      }
    }
    template < class Call, typename... Args >
    static unsigned __stdcall threadfunc(void* arg)
    {
      std::unique_ptr< Call > call(static_cast< Call* >(arg));
      call->callFunc(typename detail::GenIntSeq< sizeof...(Args) >::type());
      return 0;
    }
    bool
    joinable() const
    {
      return mHandle != _STD_THREAD_INVALID_HANDLE;
    }
    void
    join()
    {
      using namespace std;
      if(get_id() == id(GetCurrentThreadId()))
        throw system_error(
            make_error_code(errc::resource_deadlock_would_occur));
      if(mHandle == _STD_THREAD_INVALID_HANDLE)
        throw system_error(make_error_code(errc::no_such_process));
      if(!joinable())
        throw system_error(make_error_code(errc::invalid_argument));
      WaitForSingleObject(mHandle, INFINITE);
      CloseHandle(mHandle);
      mHandle = _STD_THREAD_INVALID_HANDLE;
      mThreadId.clear();
    }

    ~thread()
    {
      if(joinable())
      {
#ifndef NDEBUG
        std::printf(
            "Error: Must join() or detach() a thread before \
destroying it.\n");
#endif
        std::terminate();
      }
    }
    thread&
    operator=(const thread&) = delete;
    thread&
    operator=(thread&& other) noexcept
    {
      if(joinable())
      {
#ifndef NDEBUG
        std::printf(
            "Error: Must join() or detach() a thread before \
moving another thread to it.\n");
#endif
        std::terminate();
      }
      swap(std::forward< thread >(other));
      return *this;
    }
    void
    swap(thread&& other) noexcept
    {
      std::swap(mHandle, other.mHandle);
      std::swap(mThreadId.mId, other.mThreadId.mId);
    }

    static unsigned int
    _hardware_concurrency_helper() noexcept
    {
      SYSTEM_INFO sysinfo;
      ::GetNativeSystemInfo(&sysinfo);
      return sysinfo.dwNumberOfProcessors;
    }

    static unsigned int
    hardware_concurrency() noexcept
    {
      static unsigned int cached = _hardware_concurrency_helper();
      return cached;
    }

    void
    detach()
    {
      if(!joinable())
      {
        using namespace std;
        throw system_error(make_error_code(errc::invalid_argument));
      }
      if(mHandle != _STD_THREAD_INVALID_HANDLE)
      {
        CloseHandle(mHandle);
        mHandle = _STD_THREAD_INVALID_HANDLE;
      }
      mThreadId.clear();
    }
  };

  namespace this_thread
  {
    inline thread::id
    get_id() noexcept
    {
      return thread::id(GetCurrentThreadId());
    }
    inline void
    yield() noexcept
    {
      Sleep(0);
    }
    template < class Rep, class Period >
    void
    sleep_for(const std::chrono::duration< Rep, Period >& sleep_duration)
    {
      Sleep(std::chrono::duration_cast< std::chrono::milliseconds >(
                sleep_duration)
                .count());
    }
    template < class Clock, class Duration >
    void
    sleep_until(const std::chrono::time_point< Clock, Duration >& sleep_time)
    {
      sleep_for(sleep_time - Clock::now());
    }
  }  // namespace this_thread
}  //  Namespace mingw_stdthread

namespace std
{
//    Because of quirks of the compiler, the common "using namespace std;"
//  directive would flatten the namespaces and introduce ambiguity where there
//  was none. Direct specification (std::), however, would be unaffected.
//    Take the safe option, and include only in the presence of MinGW's win32
//  implementation.
#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
  using mingw_stdthread::thread;
  //    Remove ambiguity immediately, to avoid problems arising from the above.
  // using std::thread;
  namespace this_thread
  {
    using namespace mingw_stdthread::this_thread;
  }
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

  //    Specialize hash for this implementation's thread::id, even if the
  //  std::thread::id already has a hash.
  template <>
  struct hash< mingw_stdthread::thread::id >
  {
    typedef mingw_stdthread::thread::id argument_type;
    typedef size_t result_type;
    size_t
    operator()(const argument_type& i) const noexcept
    {
      return i.mId;
    }
  };
}  // namespace std
#endif  // WIN32STDTHREAD_H
