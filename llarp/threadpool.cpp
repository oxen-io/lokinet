#include "threadpool.hpp"
#include <iostream>

namespace llarp {
namespace thread {
Pool::Pool(size_t workers) {
  stop = false;
  while (workers--) {
    threads.emplace_back([this] {
      llarp_thread_job job;
      for (;;) {
        {
          lock_t lock(this->queue_mutex);
          this->condition.wait(
              lock, [this] { return this->stop || !this->jobs.empty(); });
          if (this->stop && this->jobs.empty()) return;
          job = std::move(this->jobs.front());
          this->jobs.pop_front();
        }
        // do work
        job.work(job.user);
        // inform result if needed
        if (job.caller) {
          if (!llarp_ev_call_async(job.caller, job.data)) {
            std::cerr << "failed to queue result in thread worker" << std::endl;
          }
        }
      }
    });
  }
}

void Pool::Join() {
  {
    lock_t lock(queue_mutex);
    stop = true;
  }
  condition.notify_all();
  for (auto &t : threads) t.join();
}

void Pool::QueueJob(const llarp_thread_job &job) {
  {
    lock_t lock(queue_mutex);

    // don't allow enqueueing after stopping the pool
    if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");

    jobs.emplace_back(job);
  }
  condition.notify_one();
}

}  // namespace thread
}  // namespace llarp

struct llarp_threadpool {
  llarp::thread::Pool impl;

  llarp_threadpool(int workers) : impl(workers) {}
};

extern "C" {

struct llarp_threadpool *llarp_init_threadpool(int workers) {
  if (workers > 0)
    return new llarp_threadpool(workers);
  else
    return nullptr;
}

void llarp_threadpool_join(struct llarp_threadpool *pool) { pool->impl.Join(); }

void llarp_threadpool_start(struct llarp_threadpool *pool) { /** no op */
}

void llarp_threadpool_queue_job(struct llarp_threadpool *pool,
                                struct llarp_thread_job job) {
  pool->impl.QueueJob(job);
}

void llarp_free_threadpool(struct llarp_threadpool **pool) {
  delete *pool;
  *pool = nullptr;
}
}
