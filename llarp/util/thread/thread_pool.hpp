#ifndef LLARP_THREAD_POOL_HPP
#define LLARP_THREAD_POOL_HPP

#include <util/thread/queue.hpp>
#include <util/thread/threading.hpp>

#include <atomic>
#include <functional>
#include <thread>
#include <vector>
#include <string_view>

namespace llarp
{
  namespace thread
  {
    class ThreadPool
    {
      // Provide an efficient fixed size threadpool. The following attributes
      // of the threadpool are fixed at construction time:
      // - the max number of pending jobs
      // - the number of threads
     public:
      using Job = std::function<void()>;
      using JobQueue = Queue<Job>;

      enum class Status
      {
        Stop,
        Run,
        Suspend,
        Drain
      };

     private:
      JobQueue m_queue;             // The job queue
      util::Semaphore m_semaphore;  // The semaphore for the queue.

      std::atomic_size_t m_idleThreads;  // Number of idle threads

      util::Mutex m_mutex;

      std::atomic<Status> m_status;

      size_t m_gateCount GUARDED_BY(m_gateMutex);
      size_t m_numThreadsReady GUARDED_BY(m_gateMutex);  // Threads ready to go through the gate.

      std::mutex m_gateMutex;
      std::condition_variable m_gateCV;
      std::condition_variable m_numThreadsCV;

      std::string m_name;
      std::vector<std::thread> m_threads;
      size_t m_createdThreads;

      void
      join();

      void
      runJobs();

      void
      drainQueue();

      void
      waitThreads();

      void
      releaseThreads();

      void
      interrupt();

      void
      worker();

      bool
      spawn();

      bool
      allThreadsReady() const REQUIRES_SHARED(m_gateMutex)
      {
        return m_numThreadsReady == m_threads.size();
      }

     public:
      ThreadPool(size_t numThreads, size_t maxJobs, std::string_view name);

      ~ThreadPool();

      // Disable the threadpool. Calls to `addJob` and `tryAddJob` will fail.
      // Jobs currently in the pool will not be affected.
      void
      disable();

      void
      enable();

      // Add a job to the bool. Note this call will block if the underlying
      // queue is full.
      // Returns false if the queue is currently disabled.
      bool
      addJob(const Job& job);
      bool
      addJob(Job&& job);

      // Try to add a job to the pool. If the queue is full, or the queue is
      // disabled, return false.
      // This call will not block.
      bool
      tryAddJob(const Job& job);
      bool
      tryAddJob(Job&& job);

      // Wait until all current jobs are complete.
      // If any jobs are submitted during this time, they **may** or **may not**
      // run.
      void
      drain();

      // Disable this pool, and cancel all pending jobs. After all currently
      // running jobs are complete, join with the threads in the pool.
      void
      shutdown();

      // Start this threadpool by spawning `threadCount()` threads.
      bool
      start();

      // Disable queueing on this threadpool and wait until all pending jobs
      // have finished.
      void
      stop();

      bool
      enabled() const;

      bool
      started() const;

      size_t
      activeThreadCount() const;

      // Current number of queued jobs
      size_t
      jobCount() const;

      // Number of threads passed in the constructor
      size_t
      threadCount() const;

      // Number of threads currently started in the threadpool
      size_t
      startedThreadCount() const;

      // Max number of queued jobs
      size_t
      capacity() const;
    };

    inline void
    ThreadPool::disable()
    {
      m_queue.disable();
    }

    inline void
    ThreadPool::enable()
    {
      m_queue.enable();
    }

    inline bool
    ThreadPool::enabled() const
    {
      return m_queue.enabled();
    }

    inline size_t
    ThreadPool::activeThreadCount() const
    {
      if (m_threads.size() == m_createdThreads)
      {
        return m_threads.size() - m_idleThreads.load(std::memory_order_relaxed);
      }

      return 0;
    }

    inline size_t
    ThreadPool::threadCount() const
    {
      return m_threads.size();
    }

    inline size_t
    ThreadPool::startedThreadCount() const
    {
      return m_createdThreads;
    }

    inline size_t
    ThreadPool::jobCount() const
    {
      return m_queue.size();
    }

    inline size_t
    ThreadPool::capacity() const
    {
      return m_queue.capacity();
    }
  }  // namespace thread
}  // namespace llarp

#endif
