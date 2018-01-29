#ifndef LLARP_THREADPOOL_HPP
#define LLARP_THREADPOOL_HPP

#include <llarp/threadpool.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace llarp {
namespace thread {
typedef std::mutex mtx_t;
typedef std::unique_lock<mtx_t> lock_t;
struct Pool {

  Pool(size_t sz);
  void QueueJob(llarp_thread_job job);
  void Join();
  std::vector<std::thread> threads;
  std::queue<llarp_thread_job> jobs;

  mtx_t queue_mutex;
  std::condition_variable condition;
  std::atomic<bool> stop;
};

} // namespace thread
} // namespace llarp

#endif
