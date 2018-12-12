#include <logger.hpp>
#include <time.hpp>
#include <timer.hpp>

#include <atomic>
#include <condition_variable>
#include <list>
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
};  // namespace llarp

struct llarp_timer_context
{
  llarp::util::Mutex timersMutex;
  std::unordered_map< uint32_t, std::unique_ptr< llarp::timer > > timers;
  std::priority_queue< std::unique_ptr< llarp::timer > > calling;
  llarp::util::Mutex tickerMutex;
  llarp::util::Condition* ticker        = nullptr;
  std::chrono::milliseconds nextTickLen = std::chrono::milliseconds(100);

  llarp_time_t m_Now;

  llarp_timer_context()
  {
    m_Now = llarp::time_now_ms();
  }

  uint32_t ids = 0;
  bool _run    = true;

  ~llarp_timer_context()
  {
    if(ticker)
      delete ticker;
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
  cancel(uint32_t id)
  {
    llarp::util::Lock lock(timersMutex);
    const auto& itr = timers.find(id);
    if(itr == timers.end())
      return;
    itr->second->canceled = true;
  }

  void
  remove(uint32_t id)
  {
    llarp::util::Lock lock(timersMutex);
    const auto& itr = timers.find(id);
    if(itr == timers.end())
      return;
    itr->second->func     = nullptr;
    itr->second->canceled = true;
  }

  uint32_t
  call_later(void* user, llarp_timer_handler_func func, uint64_t timeout_ms)
  {
    llarp::util::Lock lock(timersMutex);

    uint32_t id = ++ids;
    timers.insert(
        std::make_pair(id,
                       std::unique_ptr< llarp::timer >(
                           new llarp::timer(m_Now, timeout_ms, user, func))));
    return id;
  }

  void
  cancel_all()
  {
    std::list< uint32_t > ids;

    {
      llarp::util::Lock lock(timersMutex);

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
  t->timers.clear();
  t->stop();
  if(t->ticker)
    t->ticker->NotifyAll();
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
    llarp::util::Lock lock(t->timersMutex);
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
  t->ticker = new llarp::util::Condition();
  while(t->run())
  {
    // wait for timer mutex
    if(t->ticker)
    {
      llarp::util::Lock lock(t->tickerMutex);
      t->ticker->WaitFor(lock, t->nextTickLen);
    }

    if(t->run())
    {
      llarp::util::Lock lock(t->timersMutex);
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
    done = true;
  }
}  // namespace llarp
