#ifndef LLARP_THREADPOOL_HPP
#define LLARP_THREADPOOL_HPP

#include <llarp/threadpool.h>

#include <condition_variable>
#include <deque>
#include <mutex>
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
      Pool(size_t sz, const char* name);
      void
      QueueJob(const llarp_thread_job& job);
      void
      Join();
      void
      Stop();
      std::vector< std::thread > threads;
      std::deque< llarp_thread_job > jobs;

      mtx_t queue_mutex;
      std::condition_variable condition;
      std::condition_variable done;
      bool stop;
    };

  }  // namespace thread
}  // namespace llarp

#endif
