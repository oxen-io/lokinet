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

  void* user;
  uint64_t started;
  uint64_t timeout;
  llarp_timer_handler_func func;

  timer(uint64_t ms = 0, void* _user = nullptr,
        llarp_timer_handler_func _func = nullptr)
      : user(_user), started(now()), timeout(ms), func(_func) {}

  void operator()() {
    if (func) {
      auto ms = now();
      auto diff = ms - started;
      if (diff >= timeout)
        func(user, timeout, 0);
      else
        func(user, timeout, diff);
    }
  }
};
};  // namespace llarp

struct llarp_timer_context {
  std::mutex timersMutex;
  std::map<uint32_t, llarp::timer> timers;
  std::mutex tickerMutex;
  std::condition_variable ticker;
  std::chrono::milliseconds nextTickLen = std::chrono::seconds(1);

  uint32_t ids = 0;
  std::atomic<bool> _run = true;

  bool run() { return _run.load(); }

  void stop() { _run.store(false); }

  void cancel(uint32_t id, bool lockit = true) {
    std::unique_lock<std::mutex>* lock = nullptr;
    if (lockit) lock = new std::unique_lock<std::mutex>(timersMutex);

    auto itr = timers.find(id);
    if (itr != timers.end()) {
      itr->second();
      timers.erase(id);
    }

    if (lock) delete lock;
  }

  uint32_t call_later(void* user, llarp_timer_handler_func func,
                      uint64_t timeout_ms) {
    std::unique_lock<std::mutex> lock(timersMutex);
    uint32_t id = ids++;
    timers[id] = llarp::timer(timeout_ms);
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
  t->cancel_all();
  t->stop();
}

void llarp_timer_cancel(struct llarp_timer_context* t, uint32_t id) {
  t->cancel(id);
}

void llarp_timer_run(struct llarp_timer_context* t,
                     struct llarp_threadpool* pool) {
  std::unique_lock<std::mutex> lock(t->tickerMutex);
  while (t->run()) {
    auto status = t->ticker.wait_for(lock, t->nextTickLen);
    if (status == std::cv_status::no_timeout) {
      // we woke up
    }
  }
}
}
