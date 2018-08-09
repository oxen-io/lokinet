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
    typedef std::mutex mtx_t;
    typedef std::unique_lock< mtx_t > lock_t;
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
        llarp_thread_job* job;
        Job_t(uint32_t jobid, llarp_thread_job* j) : id(jobid), job(j)
        {
        }

        bool
        operator<(const Job_t& j) const
        {
          return id < j.id;
        }
      };

      std::priority_queue< Job_t > jobs;
      uint32_t ids = 0;
      mtx_t queue_mutex;
      std::condition_variable condition;
      std::condition_variable done;
      bool stop;
    };

    struct IsolatedPool : public Pool
    {
      IsolatedPool(int flags) : Pool(), m_flags(flags)
      {
      }

      void
      Spawn(int workers, const char* name);

      void
      Join();

      // override me to do specific setups after isolation
      // return true for success
      virtual bool
      Isolated()
      {
        return true;
      }

      std::thread* m_isolated = nullptr;
      int m_flags;
      int m_IsolatedWorkers      = 0;
      const char* m_IsolatedName = nullptr;
      char m_childstack[(1024 * 1024 * 8)];
    };

    struct NetIsolatedPool : public IsolatedPool
    {
      NetIsolatedPool(std::function< bool(void*) > setupNet, void* user);

      bool
      Isolated()
      {
        return m_NetSetup(m_user);
      }

      std::function< bool(void*) > m_NetSetup;
      void* m_user;
    };

  }  // namespace thread
}  // namespace llarp

#endif
