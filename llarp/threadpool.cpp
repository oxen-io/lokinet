#include "threadpool.hpp"
#include <pthread.h>
#include <cstring>

namespace llarp {
namespace thread {
  Pool::Pool(size_t workers, const char * name) {
  stop = false;
  while (workers--) {
    threads.emplace_back([this, name] {
      if(name)
        pthread_setname_np(pthread_self(), name);
          
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
      }
    });
  }
}

void Pool::Stop() {
  {
    lock_t lock(queue_mutex);
    stop = true;
  }
  condition.notify_all();
  done.notify_all();
}

void Pool::Join() {
  for (auto &t : threads) t.join();
  threads.clear();
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

  llarp_threadpool(int workers, const char * name) : impl(workers, name) {}
};

extern "C" {

struct llarp_threadpool *llarp_init_threadpool(int workers, const char * name) {
  if (workers > 0)
    return new llarp_threadpool(workers, name);
  else
    return nullptr;
}

void llarp_threadpool_join(struct llarp_threadpool *pool) { pool->impl.Join(); }

void llarp_threadpool_start(struct llarp_threadpool *pool) { /** no op */
}

void llarp_threadpool_stop(struct llarp_threadpool *pool) { pool->impl.Stop(); }

void llarp_threadpool_wait(struct llarp_threadpool *pool) {
  std::mutex mtx;
  {
    std::unique_lock<std::mutex> lock(mtx);
    pool->impl.done.wait(lock);
  }
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
