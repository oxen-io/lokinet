#ifndef LLARP_THREADPOOL_HPP
#define LLARP_THREADPOOL_HPP

#include <llarp/threadpool.h>
#include <llarp/threading.hpp>

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
      void
      Spawn(int workers, const char* name);

      void
      Join();

      std::thread* m_isolated    = nullptr;
      int m_IsolatedWorkers      = 0;
      const char* m_IsolatedName = nullptr;
      char m_childstack[(1024 * 1024 * 8)];
    };

  }  // namespace thread
}  // namespace llarp

#endif
