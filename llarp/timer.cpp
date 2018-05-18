#include <llarp/timer.h>
#include <atomic>
#include <condition_variable>
#include <list>
#include <map>

namespace llarp {
struct timer {
  static uint64_t now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  llarp_timer_context * parent;
  void* user;
  uint64_t started;
  uint64_t timeout;
  llarp_timer_handler_func func;
  uint32_t id;

  timer(llarp_timer_context * ctx=nullptr, uint64_t ms = 0, void* _user = nullptr,
        llarp_timer_handler_func _func = nullptr, uint32_t _id=0)
    : parent(ctx), user(_user), started(now()), timeout(ms), func(_func), id(_id) {}

  
  void exec();

  
  static void call(void * user)
  {
    static_cast<timer *>(user)->exec();
  }

  operator llarp_thread_job ()
  {
    return {this, timer::call};
  }
  
};
};  // namespace llarp

struct llarp_timer_context {
  llarp_threadpool * threadpool;
  std::mutex timersMutex;
  std::map<uint32_t, llarp::timer> timers;
  std::mutex tickerMutex;
  std::condition_variable ticker;
  std::chrono::milliseconds nextTickLen = std::chrono::milliseconds(10);

  uint32_t ids = 0;
  std::atomic<bool> _run = true;

  bool run() { return _run.load(); }

  void stop() { _run.store(false); }

  void cancel(uint32_t id, bool lockit = true) {
    std::unique_lock<std::mutex>* lock = nullptr;
    if (lockit) lock = new std::unique_lock<std::mutex>(timersMutex);

    auto itr = timers.find(id);
    if (itr != timers.end()) {
      itr->second.exec();
    }

    if (lock) delete lock;
  }

  void remove(uint32_t id)
  {
    std::unique_lock<std::mutex> lock (timersMutex);
    timers.erase(id);
  }

  uint32_t call_later(void* user, llarp_timer_handler_func func,
                      uint64_t timeout_ms) {
    std::unique_lock<std::mutex> lock(timersMutex);
    uint32_t id = ++ids;
    timers[id] = llarp::timer(this, timeout_ms, user, func, id);
    return id;
  }

  void cancel_all() {
    std::unique_lock<std::mutex> lock(timersMutex);

    std::list<uint32_t> ids;

    for (auto& item : timers) {
      ids.push_back(item.first);
    }

    for (auto id : ids) {
      cancel(id, false);
    }
  }
};

extern "C" {

struct llarp_timer_context* llarp_init_timer() {
  return new llarp_timer_context;
}

uint32_t llarp_timer_call_later(struct llarp_timer_context* t,
                                struct llarp_timeout_job job) {
  return t->call_later(job.user, job.handler, job.timeout);
}

void llarp_free_timer(struct llarp_timer_context** t) {
  if (*t) delete *t;
  *t = nullptr;
}

void llarp_timer_stop(struct llarp_timer_context* t) {
  t->stop();
  {
    std::unique_lock<std::mutex> lock(t->tickerMutex);
  
    auto itr = t->timers.begin();
    while (itr != t->timers.end())
    {
      // timer expired
      llarp_threadpool_queue_job(t->threadpool, itr->second);
      ++itr;
    }
  }
}

void llarp_timer_cancel(struct llarp_timer_context* t, uint32_t id) {
  t->cancel(id);
}

void llarp_timer_run(struct llarp_timer_context* t,
                     struct llarp_threadpool* pool) {
  t->threadpool = pool;
  while (t->run()) {
    std::unique_lock<std::mutex> lock(t->tickerMutex);
    t->ticker.wait_for(lock, t->nextTickLen);
    // we woke up
    auto now = llarp::timer::now();
    auto itr = t->timers.begin();
    while (itr != t->timers.end())
    {
      if(now - itr->second.started >= itr->second.timeout)
      {
        // timer hit
        llarp_threadpool_queue_job(pool, itr->second);
      }
      ++itr;
    }
  }
}
}

namespace llarp
{
  void timer::exec()
  {
    if (func) {
      auto ms = now();
      auto diff = ms - started;
      if (diff >= timeout)
        func(user, timeout, 0);
      else
        func(user, timeout, diff);
    }
    if(parent)
      parent->remove(id);
  }
}
