#ifndef LLARP_THREADPOOL_HPP
#define LLARP_THREADPOOL_HPP

#include <util/thread_pool.hpp>
#include <util/threading.hpp>
#include <util/threadpool.h>

namespace llarp
{
  namespace thread
  {
    using Pool = ThreadPool;

    struct IsolatedPool : public Pool
    {
      IsolatedPool(size_t workers, int flags)
          : Pool(workers, workers * 128), m_flags(flags)
      {
      }

      void
      Join();

      /// called when isolation failed
      virtual void
      Fail()
      {
      }

      std::thread* m_isolated = nullptr;
      int m_flags;
      int m_IsolatedWorkers    = 0;
      const char* IsolatedName = nullptr;

      virtual void
      MainLoop()
      {
      }
    };

    struct _NetIsolatedPool : public IsolatedPool
    {
      _NetIsolatedPool(std::function< bool(void*, bool) > setupNet,
                       std::function< void(void*) > runMain, void* user);

      /// implement me per platform
      virtual bool
      IsolateNetwork() = 0;

      void
      Fail()
      {
        m_NetSetup(m_user, false);
      }

      void
      MainLoop()
      {
        m_RunMain(m_user);
      }

      std::function< bool(void*, bool) > m_NetSetup;
      std::function< void(void*) > m_RunMain;
      void* m_user;
    };

  }  // namespace thread
}  // namespace llarp

#endif
