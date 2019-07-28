#include <util/logger.hpp>
#include <util/time.hpp>
#include <util/timer.hpp>

#include <atomic>
#include <condition_variable>
#include <list>
#include <memory>
#include <queue>
#include <unordered_map>

namespace llarp
{
  struct timer
  {
    void* user;
    uint64_t called_at;
    uint64_t started;
    uint64_t timeout;
    llarp_timer_handler_func func;
    std::function< void(void) > deferredFunc;
    bool done;
    bool canceled;

    timer(llarp_time_t now, uint64_t ms = 0, void* _user = nullptr,
          llarp_timer_handler_func _func = nullptr)
        : user(_user)
        , called_at(0)
        , started(now)
        , timeout(ms)
        , func(_func)
        , done(false)
        , canceled(false)
    {
    }

    ~timer()
    {
    }

    void
    exec();

    static void
    call(void* user)
    {
      static_cast< timer* >(user)->exec();
    }

    bool
    operator<(const timer& other) const
    {
      return (started + timeout) < (other.started + other.timeout);
    }
  };
}  // namespace llarp

struct llarp_timer_context
{
  llarp::util::Mutex timersMutex;  // protects timers
  std::unordered_map< uint32_t, std::unique_ptr< llarp::timer > > timers
      GUARDED_BY(timersMutex);
  std::priority_queue< std::unique_ptr< llarp::timer > > calling;
  llarp::util::Mutex tickerMutex;
  std::unique_ptr< llarp::util::Condition > ticker;
  absl::Duration nextTickLen = absl::Milliseconds(100);

  llarp_time_t m_Now;

  llarp_timer_context()
  {
    m_Now = llarp::time_now_ms();
  }

  uint32_t currentId = 0;
  bool _run          = true;

  ~llarp_timer_context()
  {
  }

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
  cancel(uint32_t id) LOCKS_EXCLUDED(timersMutex)
  {
    llarp::util::Lock lock(&timersMutex);
    const auto& itr = timers.find(id);
    if(itr == timers.end())
      return;
    itr->second->canceled = true;
  }

  void
  remove(uint32_t id) LOCKS_EXCLUDED(timersMutex)
  {
    llarp::util::Lock lock(&timersMutex);
    const auto& itr = timers.find(id);
    if(itr == timers.end())
      return;
    itr->second->func     = nullptr;
    itr->second->canceled = true;
  }

  uint32_t
  call_later(void* user, llarp_timer_handler_func func, uint64_t timeout_ms)
      LOCKS_EXCLUDED(timersMutex)
  {
    llarp::util::Lock lock(&timersMutex);

    const uint32_t id = ++currentId;
    timers.emplace(
        id, std::make_unique< llarp::timer >(m_Now, timeout_ms, user, func));
    return id;
  }

  uint32_t
  call_func_later(std::function< void(void) > func, llarp_time_t timeout)
  {
    llarp::util::Lock lock(&timersMutex);

    const uint32_t id = ++currentId;
    timers.emplace(
        id, std::make_unique< llarp::timer >(m_Now, timeout, nullptr, nullptr));
    timers[id]->deferredFunc = func;
    return id;
  }

  void
  cancel_all() LOCKS_EXCLUDED(timersMutex)
  {
    std::list< uint32_t > ids;

    {
      llarp::util::Lock lock(&timersMutex);

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

uint32_t
llarp_timer_call_func_later(struct llarp_timer_context* t, llarp_time_t timeout,
                            std::function< void(void) > func)
{
  return t->call_func_later(func, timeout);
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
  {
    llarp::util::Lock lock(&t->timersMutex);
    t->timers.clear();
    t->stop();
  }
  if(t->ticker)
    t->ticker->SignalAll();
}

void
llarp_timer_cancel_job(struct llarp_timer_context* t, uint32_t id)
{
  t->cancel(id);
}

void
llarp_timer_set_time(struct llarp_timer_context* t, llarp_time_t now)
{
  if(now == 0)
    now = llarp::time_now_ms();
  t->m_Now = now;
}

void
llarp_timer_tick_all(struct llarp_timer_context* t)
{
  if(!t->run())
    return;

  std::list< std::unique_ptr< llarp::timer > > hit;
  {
    llarp::util::Lock lock(&t->timersMutex);
    auto itr = t->timers.begin();
    while(itr != t->timers.end())
    {
      if(t->m_Now - itr->second->started >= itr->second->timeout
         || itr->second->canceled)
      {
        // timer hit
        hit.emplace_back(std::move(itr->second));
        itr = t->timers.erase(itr);
      }
      else
        ++itr;
    }
  }
  for(const auto& h : hit)
  {
    if(h->func)
    {
      h->called_at = t->m_Now;
      h->exec();
    }
  }
}

static void
llarp_timer_tick_all_job(void* user)
{
  llarp_timer_tick_all(static_cast< llarp_timer_context* >(user));
}

void
llarp_timer_tick_all_async(struct llarp_timer_context* t,
                           struct llarp_threadpool* pool, llarp_time_t now)
{
  t->m_Now = now;
  llarp_threadpool_queue_job(pool, {t, llarp_timer_tick_all_job});
}

void
llarp_timer_run(struct llarp_timer_context* t, struct llarp_threadpool* pool)
{
  t->ticker = std::make_unique< llarp::util::Condition >();
  while(t->run())
  {
    // wait for timer mutex
    if(t->ticker)
    {
      llarp::util::Lock lock(&t->tickerMutex);
      t->ticker->WaitWithTimeout(&t->tickerMutex, t->nextTickLen);
    }

    if(t->run())
    {
      llarp::util::Lock lock(&t->timersMutex);
      // we woke up
      llarp_timer_tick_all_async(t, pool, llarp::time_now_ms());
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
    }
    if(deferredFunc)
      deferredFunc();
    done = true;
  }
}  // namespace llarp
