#include <llarp/timer.h>
#include <atomic>
#include <condition_variable>
#include <list>
#include <unordered_map>

#include "logger.hpp"

namespace llarp
{
  struct timer
  {
    static uint64_t
    now()
    {
      return std::chrono::duration_cast< std::chrono::milliseconds >(
                 std::chrono::steady_clock::now().time_since_epoch())
          .count();
    }

    void* user;
    uint64_t called_at;
    uint64_t started;
    uint64_t timeout;
    llarp_timer_handler_func func;
    bool done;

    timer(uint64_t ms = 0, void* _user = nullptr,
          llarp_timer_handler_func _func = nullptr)
        : user(_user)
        , called_at(0)
        , started(now())
        , timeout(ms)
        , func(_func)
        , done(false)
    {
    }

    void
    exec();

    static void
    call(void* user)
    {
      static_cast< timer* >(user)->exec();
    }

    llarp_thread_job
    to_job()
    {
      return {this, timer::call};
    }
  };
};  // namespace llarp

struct llarp_timer_context
{
  llarp_threadpool* threadpool;
  std::mutex timersMutex;
  std::unordered_map< uint32_t, llarp::timer* > timers;
  std::mutex tickerMutex;
  std::condition_variable ticker;
  std::chrono::milliseconds nextTickLen = std::chrono::milliseconds(10);

  uint32_t ids = 0;
  bool _run    = true;

  bool
  run()
  {
    return _run;
  }

  void
  stop()
  {
    _run = false;
  }

  void
  cancel(uint32_t id)
  {
    llarp::timer* t;
    {
      std::unique_lock< std::mutex > lock(timersMutex);
      auto itr = timers.find(id);
      if(itr == timers.end())
        return;
      t = itr->second;
      timers.erase(itr);
    }
    t->called_at = llarp::timer::now();
    t->exec();
    delete t;
  }

  void
  remove(uint32_t id)
  {
    std::unique_lock< std::mutex > lock(timersMutex);
    auto itr = timers.find(id);
    if(itr != timers.end())
    {
      delete itr->second;
      timers.erase(itr);
    }
  }

  uint32_t
  call_later(void* user, llarp_timer_handler_func func, uint64_t timeout_ms)
  {
    std::unique_lock< std::mutex > lock(timersMutex);
    uint32_t id = ++ids;
    timers.emplace(id, new llarp::timer(timeout_ms, user, func));
    return id;
  }

  void
  cancel_all()
  {
    std::list< uint32_t > ids;

    {
      std::unique_lock< std::mutex > lock(timersMutex);

      for(auto& item : timers)
      {
        ids.push_back(item.first);
      }
    }

    for(auto id : ids)
    {
      cancel(id);
    }
  }
};

extern "C" {

struct llarp_timer_context*
llarp_init_timer()
{
  return new llarp_timer_context;
}

uint32_t
llarp_timer_call_later(struct llarp_timer_context* t,
                       struct llarp_timeout_job job)
{
  return t->call_later(job.user, job.handler, job.timeout);
}

void
llarp_free_timer(struct llarp_timer_context** t)
{
  if(*t)
    delete *t;
  *t = nullptr;
}

void
llarp_timer_remove_job(struct llarp_timer_context* t, uint32_t id)
{
  t->remove(id);
}

void
llarp_timer_stop(struct llarp_timer_context* t)
{
  // destroy all timers
  // don't call callbacks on timers
  llarp::Debug("clear timers");
  t->timers.clear();
  t->stop();
  llarp::Debug("stop timers");
  t->ticker.notify_all();
}

void
llarp_timer_cancel_job(struct llarp_timer_context* t, uint32_t id)
{
  t->cancel(id);
}

void
llarp_timer_run(struct llarp_timer_context* t, struct llarp_threadpool* pool)
{
  t->threadpool = pool;
  while(t->run())
  {
    // wait for timer mutex
    {
      std::unique_lock< std::mutex > lock(t->tickerMutex);
      t->ticker.wait_for(lock, t->nextTickLen);
    }

    if(t->run())
    {
      std::unique_lock< std::mutex > lock(t->timersMutex);
      // we woke up
      auto now = llarp::timer::now();
      auto itr = t->timers.begin();
      while(itr != t->timers.end())
      {
        if(now - itr->second->started >= itr->second->timeout)
        {
          if(itr->second->func && itr->second->called_at == 0)
          {
            // timer hit
            itr->second->called_at = now;
            llarp_threadpool_queue_job(pool, itr->second->to_job());
            ++itr;
          }
          else if(itr->second->done)
          {
            // timer was already called, remove timer
            delete itr->second;
            itr = t->timers.erase(itr);
          }
          else
            ++itr;
        }
        else  // timer not hit yet
          ++itr;
      }
    }
  }
}
}

namespace llarp
{
  void
  timer::exec()
  {
    if(func)
    {
      auto diff = called_at - started;
      // zero out function pointer before call to prevent multiple calls being
      // queued if call takes longer than 1 timer tick
      auto call = func;
      func      = nullptr;
      if(diff >= timeout)
        call(user, timeout, 0);
      else
        call(user, timeout, diff);
      done = true;
    }
  }
}
