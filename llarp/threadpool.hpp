#ifndef LLARP_THREADPOOL_HPP
#define LLARP_THREADPOOL_HPP

#include <llarp/threadpool.h>
#include <llarp/threading.hpp>

#include <functional>
#include <queue>

#include <thread>
#include <vector>

namespace llarp
{
  namespace thread
  {
    typedef util::Mutex mtx_t;
    typedef util::Lock lock_t;
    struct Pool
    {
      virtual void
      Spawn(size_t sz, const char* name);

      void
      QueueJob(const llarp_thread_job& job);

      virtual void
      Join();

      void
      Stop();
      std::vector< std::thread > threads;

      struct Job_t
      {
        uint32_t id;
        void* user;
        llarp_thread_work_func work;

        Job_t() = default;

        Job_t(uint32_t jobid, const llarp_thread_job& j)
            : id(jobid), user(j.user), work(j.work)
        {
        }

        bool
        operator<(const Job_t& j) const
        {
          return id < j.id;
        }

        void
        operator()() const
        {
          work(user);
        }
      };

      std::priority_queue< Job_t > jobs;
      uint32_t ids = 0;
      mtx_t queue_mutex;
      util::Condition condition;
      util::Condition done;
      bool stop;
    };

    struct IsolatedPool : public Pool
    {
      IsolatedPool(int flags) : Pool(), m_flags(flags)
      {
      }

      void
      Spawn(size_t workers, const char* name);

      void
      Join();

      /// isolate current thread
      /// return true for success
      /// return false for failure
      /// set errno on fail
      /// override me in subclass
      virtual bool
      IsolateCurrentProcess()
      {
        return true;
      }

      // override me to do specific setups after isolation
      // return true for success
      virtual bool
      Isolated()
      {
        return true;
      }

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

      bool
      IsolateCurrentProcess()
      {
        return IsolateNetwork();
      }

      bool
      Isolated()
      {
        return m_NetSetup(m_user, true);
      }

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
