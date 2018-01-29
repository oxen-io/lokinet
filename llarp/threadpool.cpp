#include "threadpool.hpp"
#include <iostream>

namespace llarp {
namespace thread {
Pool::Pool(size_t workers) {
  stop.store(true);
  while (workers--) {
    threads.emplace_back([this] {
      for (;;) {
        llarp_thread_job job;
        {
          lock_t lock(this->queue_mutex);
          this->condition.wait(
              lock, [this] { return this->stop || !this->jobs.empty(); });
          if (this->stop && this->jobs.empty())
            return;
          job = std::move(this->jobs.front());
          this->jobs.pop();
        }
        // do work
        job.work(job.user);
        // inform result if needed
        if (job.result && job.result->loop)
          if (!llarp_ev_async(job.result->loop, *job.result)) {
            std::cerr << "failed to queue result in thread worker" << std::endl;
          }
      }
    });
  }
}

void Pool::Join() {
  {
    lock_t lock(queue_mutex);
    stop.store(true);
  }
  condition.notify_all();
  for (auto &t : threads)
    t.join();
}

void Pool::QueueJob(llarp_thread_job job) {
  {
    lock_t lock(queue_mutex);

    // don't allow enqueueing after stopping the pool
    if (stop)
      throw std::runtime_error("enqueue on stopped ThreadPool");

    jobs.emplace(job);
  }
  condition.notify_one();
}

} // namespace thread
} // namespace llarp

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

void llarp_free_threadpool(struct llarp_threadpool **pool) {
  delete *pool;
  *pool = nullptr;
}
}
