#include "threadpool.hpp"
#ifndef _MSC_VER
#include <pthread.h>
#endif
#include <cstring>

#include <llarp/time.h>
#include <functional>
#include <queue>

#include "logger.hpp"

#if(__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
#include <pthread_np.h>
#endif

#ifdef __linux__
#include <llarp/linux/netns.hpp>
#endif

#ifdef _MSC_VER
#include <windows.h>
extern "C" void
SetThreadName(DWORD dwThreadID, LPCSTR szThreadName);
#endif

namespace llarp
{
  namespace thread
  {
    void
    Pool::Spawn(size_t workers, const char *name)
    {
      stop = false;
      while(workers--)
      {
        threads.emplace_back([this, name] {
          if(name)
          {
#if(__APPLE__ && __MACH__)
            pthread_setname_np(name);
#elif(__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
            pthread_set_name_np(pthread_self(), name);
#elif(__linux__) || (__MINGW32__)
            pthread_setname_np(pthread_self(), name);
#elif defined(_MSC_VER)
            SetThreadName(GetCurrentThreadId(), name);
#endif
          }
          for(;;)
          {
            Job_t job;
            {
              lock_t lock(this->queue_mutex);
              this->condition.WaitUntil(
                  lock, [this] { return this->stop || !this->jobs.empty(); });
              if(this->stop)
              {
                // discard pending jobs
                while(this->jobs.size())
                {
                  this->jobs.pop();
                }
                return;
              }
              job = std::move(this->jobs.top());
              this->jobs.pop();
            }
            // do work
            job();
          }
        });
      }
    }

    void
    Pool::Stop()
    {
      {
        lock_t lock(queue_mutex);
        stop = true;
      }
      condition.NotifyAll();
    }

    void
    Pool::Join()
    {
      for(auto &t : threads)
        t.join();
      threads.clear();
      done.NotifyAll();
    }

    void
    Pool::QueueJob(const llarp_thread_job &job)
    {
      {
        lock_t lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
          return;
        jobs.emplace(ids++, job);
      }
      condition.NotifyOne();
    }

    void
    IsolatedPool::Spawn(size_t workers, const char *name)
    {
      IsolatedPool *self      = this;
      self->IsolatedName      = name;
      self->m_IsolatedWorkers = workers;
      m_isolated              = new std::thread([self] {
        if(!self->IsolateCurrentProcess())
        {
          llarp::LogError("isolation failed: ", strerror(errno));
          self->Fail();
          return;
        }
        llarp::LogInfo("spawning isolated environment");
        self->Pool::Spawn(self->m_IsolatedWorkers, self->IsolatedName);
        if(self->Isolated())
        {
          self->MainLoop();
        }
      });
    }

    void
    IsolatedPool::Join()
    {
      Pool::Join();
      if(m_isolated)
      {
        m_isolated->join();
        delete m_isolated;
        m_isolated = nullptr;
      }
    }

    _NetIsolatedPool::_NetIsolatedPool(
        std::function< bool(void *, bool) > setupNet,
        std::function< void(void *) > runMain, void *user)
        : IsolatedPool(0)

    {
      m_NetSetup = setupNet;
      m_RunMain  = runMain;
      m_user     = user;
    }

#ifdef __linux__
    struct LinuxNetNSIsolatedPool : public _NetIsolatedPool
    {
      LinuxNetNSIsolatedPool(std::function< bool(void *, bool) > setup,
                             std::function< void(void *) > run, void *user)
          : _NetIsolatedPool(setup, run, user)
      {
      }

      bool
      IsolateNetwork()
      {
        return ::llarp::GNULinux::NetNSSwitch(IsolatedName);
      }
    };

    typedef LinuxNetNSIsolatedPool NetIsolatedPool;
#define NET_ISOLATION_SUPPORTED
#endif

#if defined(__FreeBSD__)
    struct FreeBSDJailedThreadPool : public _NetIsolatedPool
    {
      FreeBSDJailedThreadPool(std::function< bool(void *, bool) > setup,
                              std::function< void(void *) > run, void *user)
          : _NetIsolatedPool(setup, run, user)
      {
      }

      bool
      IsolateNetwork()
      {
        // TODO: implement me
        return false;
      }
    };
    typedef FreeBSDJailedThreadPool NetIsolatedPool;
#define NET_ISOLATION_SUPPORTED
#endif

  }  // namespace thread
}  // namespace llarp

struct llarp_threadpool
{
  llarp::thread::Pool *impl;

  llarp::util::Mutex m_access;
  uint32_t ids = 0;
  std::queue< llarp::thread::Pool::Job_t > jobs;

  llarp_threadpool(int workers, const char *name, bool isolate,
                   setup_net_func setup  = nullptr,
                   run_main_func runmain = nullptr, void *user = nullptr)
  {
#ifdef NET_ISOLATION_SUPPORTED
    if(isolate)
      impl = new llarp::thread::NetIsolatedPool(setup, runmain, user);
    else
#else
    if(isolate)
    {
      llarp::LogError("network isolation not supported");
    }
#endif
      impl = new llarp::thread::Pool();
    impl->Spawn(workers, name);
  }

  llarp_threadpool() : impl(nullptr)
  {
  }
};

struct llarp_threadpool *
llarp_init_threadpool(int workers, const char *name)
{
  if(workers <= 0)
    workers = 1;
  return new llarp_threadpool(workers, name, false);
}

struct llarp_threadpool *
llarp_init_same_process_threadpool()
{
  return new llarp_threadpool();
}

struct llarp_threadpool *
llarp_init_isolated_net_threadpool(const char *name, setup_net_func setup,
                                   run_main_func runmain, void *context)
{
  return new llarp_threadpool(1, name, true, setup, runmain, context);
}

void
llarp_threadpool_join(struct llarp_threadpool *pool)
{
  llarp::LogDebug("threadpool join");
  if(pool->impl)
    pool->impl->Join();
}

void
llarp_threadpool_start(struct llarp_threadpool *pool)
{ /** no op */
}

void
llarp_threadpool_stop(struct llarp_threadpool *pool)
{
  llarp::LogDebug("threadpool stop");
  if(pool->impl)
    pool->impl->Stop();
}

void
llarp_threadpool_wait(struct llarp_threadpool *pool)
{
  llarp::util::Mutex mtx;
  llarp::LogDebug("threadpool wait");
  if(pool->impl)
  {
    llarp::util::Lock lock(mtx);
    pool->impl->done.Wait(lock);
  }
}

void
llarp_threadpool_queue_job(struct llarp_threadpool *pool,
                           struct llarp_thread_job job)
{
  if(pool->impl)
    pool->impl->QueueJob(job);
  else
  {
    // single threaded mode
    llarp::util::Lock lock(pool->m_access);
    pool->jobs.emplace(++pool->ids, job);
  }
}

void
llarp_threadpool_tick(struct llarp_threadpool *pool)
{
  while(pool->jobs.size())
  {
    llarp::thread::Pool::Job_t job;
    {
      llarp::util::Lock lock(pool->m_access);
      job = std::move(pool->jobs.front());
      pool->jobs.pop();
    }
    job();
  }
}

void
llarp_free_threadpool(struct llarp_threadpool **pool)
{
  if(*pool)
  {
    delete *pool;
  }
  *pool = nullptr;
}
