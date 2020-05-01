#include <util/thread/thread_pool.hpp>

#include <util/thread/threading.hpp>

namespace llarp
{
  namespace thread
  {
    void
    ThreadPool::join()
    {
      for (auto& t : m_threads)
      {
        if (t.joinable())
        {
          t.join();
        }
      }

      m_createdThreads = 0;
    }

    void
    ThreadPool::runJobs()
    {
      while (m_status.load(std::memory_order_relaxed) == Status::Run)
      {
        auto functor = m_queue.tryPopFront();

        if (functor.has_value())
        {
          functor.value()();
        }
        else
        {
          m_idleThreads++;

          if (m_status == Status::Run && m_queue.empty())
          {
            m_semaphore.wait();
          }

          m_idleThreads.fetch_sub(1, std::memory_order_relaxed);
        }
      }
    }

    void
    ThreadPool::drainQueue()
    {
      while (m_status.load(std::memory_order_relaxed) == Status::Drain)
      {
        auto functor = m_queue.tryPopFront();

        if (!functor)
        {
          return;
        }

        functor.value()();
      }
    }

    void
    ThreadPool::waitThreads()
    {
      std::unique_lock<std::mutex> lock(m_gateMutex);
      m_numThreadsCV.wait(lock, [this] { return allThreadsReady(); });
    }

    void
    ThreadPool::releaseThreads()
    {
      {
        std::lock_guard<std::mutex> lock(m_gateMutex);
        m_numThreadsReady = 0;
        ++m_gateCount;
      }
      m_gateCV.notify_all();
    }

    void
    ThreadPool::interrupt()
    {
      std::lock_guard<std::mutex> lock(m_gateMutex);

      size_t count = m_idleThreads;

      for (size_t i = 0; i < count; ++i)
      {
        m_semaphore.notify();
      }
    }

    void
    ThreadPool::worker()
    {
      // Lock will be valid until the end of the statement
      size_t gateCount = (std::lock_guard<std::mutex>(m_gateMutex), m_gateCount);

      util::SetThreadName(m_name);

      for (;;)
      {
        {
          std::unique_lock<std::mutex> lock(m_gateMutex);
          ++m_numThreadsReady;
          m_numThreadsCV.notify_one();

          m_gateCV.wait(lock, [&] { return gateCount != m_gateCount; });

          gateCount = m_gateCount;
        }

        Status status = m_status.load(std::memory_order_relaxed);

        // Can't use a switch here as we want to load and fall through.

        if (status == Status::Run)
        {
          runJobs();
          status = m_status;
        }

        if (status == Status::Drain)
        {
          drainQueue();
        }
        else if (status == Status::Suspend)
        {
          continue;
        }
        else
        {
          assert(status == Status::Stop);
          return;
        }
      }
    }

    bool
    ThreadPool::spawn()
    {
      try
      {
        m_threads.at(m_createdThreads) = std::thread(std::bind(&ThreadPool::worker, this));
        ++m_createdThreads;
        return true;
      }
      catch (const std::system_error&)
      {
        return false;
      }
    }

    ThreadPool::ThreadPool(size_t numThreads, size_t maxJobs, std::string_view name)
        : m_queue(maxJobs)
        , m_semaphore(0)
        , m_idleThreads(0)
        , m_status(Status::Stop)
        , m_gateCount(0)
        , m_numThreadsReady(0)
        , m_name(name)
        , m_threads(numThreads)
        , m_createdThreads(0)
    {
      assert(numThreads != 0);
      assert(maxJobs != 0);
      disable();
    }

    ThreadPool::~ThreadPool()
    {
      shutdown();
    }

    bool
    ThreadPool::addJob(const Job& job)
    {
      assert(job);

      QueueReturn ret = m_queue.pushBack(job);

      if (ret == QueueReturn::Success && m_idleThreads > 0)
      {
        m_semaphore.notify();
      }

      return ret == QueueReturn::Success;
    }
    bool
    ThreadPool::addJob(Job&& job)
    {
      assert(job);
      QueueReturn ret = m_queue.pushBack(std::move(job));

      if (ret == QueueReturn::Success && m_idleThreads > 0)
      {
        m_semaphore.notify();
      }

      return ret == QueueReturn::Success;
    }

    bool
    ThreadPool::tryAddJob(const Job& job)
    {
      assert(job);
      QueueReturn ret = m_queue.tryPushBack(job);

      if (ret == QueueReturn::Success && m_idleThreads > 0)
      {
        m_semaphore.notify();
      }

      return ret == QueueReturn::Success;
    }

    bool
    ThreadPool::tryAddJob(Job&& job)
    {
      assert(job);
      QueueReturn ret = m_queue.tryPushBack(std::move(job));

      if (ret == QueueReturn::Success && m_idleThreads > 0)
      {
        m_semaphore.notify();
      }

      return ret == QueueReturn::Success;
    }

    void
    ThreadPool::drain()
    {
      util::Lock lock(m_mutex);

      if (m_status.load(std::memory_order_relaxed) == Status::Run)
      {
        m_status = Status::Drain;

        interrupt();
        waitThreads();

        m_status = Status::Run;

        releaseThreads();
      }
    }

    void
    ThreadPool::shutdown()
    {
      util::Lock lock(m_mutex);

      if (m_status.load(std::memory_order_relaxed) == Status::Run)
      {
        m_queue.disable();
        m_status = Status::Stop;

        interrupt();
        m_queue.removeAll();

        join();
      }
    }

    bool
    ThreadPool::start()
    {
      util::Lock lock(m_mutex);

      if (m_status.load(std::memory_order_relaxed) != Status::Stop)
      {
        return true;
      }

      for (auto it = (m_threads.begin() + m_createdThreads); it != m_threads.end(); ++it)
      {
        if (!spawn())
        {
          releaseThreads();

          join();

          return false;
        }
      }

      waitThreads();

      m_queue.enable();
      m_status = Status::Run;

      // `releaseThreads` has a release barrier so workers don't return from
      // wait and not see the above store.

      releaseThreads();

      return true;
    }

    void
    ThreadPool::stop()
    {
      util::Lock lock(m_mutex);

      if (m_status.load(std::memory_order_relaxed) == Status::Run)
      {
        m_queue.disable();
        m_status = Status::Drain;

        // `interrupt` has an acquire barrier (locks a mutex), so nothing will
        // be executed before the above store to `status`.
        interrupt();

        waitThreads();

        m_status = Status::Stop;

        // `releaseThreads` has a release barrier so workers don't return from
        // wait and not see the above store.

        releaseThreads();

        join();
      }
    }

  }  // namespace thread
}  // namespace llarp
