#include <util/thread/timer.hpp>
#include <util/logging/logger.hpp>
#include <util/time.hpp>

#include <atomic>
#include <condition_variable>
#include <list>
#include <memory>
#include <queue>
#include <unordered_map>
#include <utility>

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
        , func(std::move(_func))
        , done(false)
        , canceled(false)
    {
    }

    ~timer() = default;

    void
    exec();

    static void
    call(void* user)
    {
      static_cast< timer* >(user)->exec();
    }
  };
}  // namespace llarp

struct llarp_timer_context
{
  llarp::util::Mutex timersMutex;  // protects timers
  std::unordered_map< uint32_t, std::unique_ptr< llarp::timer > > timers
      GUARDED_BY(timersMutex);
  llarp::util::Mutex tickerMutex;
  std::unique_ptr< llarp::util::Condition > ticker;
  absl::Duration nextTickLen = absl::Milliseconds(100);

  llarp_time_t m_Now;
  llarp_time_t m_NextRequiredTickAt =
      std::numeric_limits< llarp_time_t >::max();
  size_t m_NumPendingTimers;

  llarp_timer_context()
  {
    m_Now = llarp::time_now_ms();
  }

  uint32_t currentId = 0;
  bool _run          = true;

  ~llarp_timer_context() = default;

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
    m_NextRequiredTickAt = std::min(m_NextRequiredTickAt, m_Now + timeout_ms);
    m_NumPendingTimers   = timers.size();
    return id;
  }

  uint32_t
  call_func_later(std::function< void(void) > func, llarp_time_t timeout_ms)
  {
    llarp::util::Lock lock(&timersMutex);

    const uint32_t id = ++currentId;
    timers.emplace(
        id,
        std::make_unique< llarp::timer >(m_Now, timeout_ms, nullptr, nullptr));
    timers[id]->deferredFunc = func;
    m_NextRequiredTickAt = std::min(m_NextRequiredTickAt, m_Now + timeout_ms);
    m_NumPendingTimers   = timers.size();
    return id;
  }

  void
  cancel_all() LOCKS_EXCLUDED(timersMutex)
  {
    {
      llarp::util::Lock lock(&timersMutex);

      for(auto& item : timers)
      {
        item.second->func     = nullptr;
        item.second->canceled = true;
      }
    }
  }

  bool
  ShouldTriggerTimers(llarp_time_t peekAhead) const
  {
    return m_NumPendingTimers > 0
        and (m_Now + peekAhead) >= m_NextRequiredTickAt;
  }
};

struct llarp_timer_context*
llarp_init_timer()
{
  return new llarp_timer_context();
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
llarp_free_timer(struct llarp_timer_context* t)
{
  delete t;
}

void
llarp_timer_remove_job(struct llarp_timer_context* t, uint32_t id)
{
  t->remove(id);
}

void
llarp_timer_stop(struct llarp_timer_context* t)
{
  llarp::LogDebug("timers stopping");
  // destroy all timers
  // don't call callbacks on timers
  {
    llarp::util::Lock lock(&t->timersMutex);
    t->timers.clear();
    t->stop();
  }
  if(t->ticker)
    t->ticker->SignalAll();
  llarp::LogDebug("timers stopped");
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
  const auto now = llarp::time_now_ms();
  t->m_Now       = now;
  std::list< std::unique_ptr< llarp::timer > > hit;
  {
    llarp::util::Lock lock(&t->timersMutex);
    auto itr = t->timers.begin();
    while(itr != t->timers.end())
    {
      if(now - itr->second->started >= itr->second->timeout
         || itr->second->canceled)
      {
        // timer hit
        hit.emplace_back(std::move(itr->second));
        itr = t->timers.erase(itr);
      }
      else
      {
        ++itr;
      }
    }
  }
  while(not hit.empty())
  {
    const auto& h = hit.front();
    h->called_at  = now;
    h->exec();
    hit.pop_front();
  }
  // reindex next tick info
  {
    llarp::util::Lock lock(&t->timersMutex);
    t->m_Now                = now;
    t->m_NextRequiredTickAt = std::numeric_limits< llarp_time_t >::max();
    for(const auto& item : t->timers)
    {
      t->m_NextRequiredTickAt =
          std::min(t->m_NextRequiredTickAt, item.second->timeout + t->m_Now);
    }
    t->m_NumPendingTimers = t->timers.size();
  }
}

bool
llarp_timer_should_call(struct llarp_timer_context* t)
{
  return t->ShouldTriggerTimers(0);
}

void
llarp_timer_tick_all_async(struct llarp_timer_context* t,
                           struct llarp_threadpool* pool, llarp_time_t now)
{
  llarp_timer_set_time(t, now);
  if(t->ShouldTriggerTimers(0))
    llarp_threadpool_queue_job(pool, std::bind(&llarp_timer_tick_all, t));
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
    if(deferredFunc && not canceled)
      deferredFunc();
    deferredFunc = nullptr;
    done         = true;
  }
}  // namespace llarp
