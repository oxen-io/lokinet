#include <util/thread/thread_pool.hpp>
#include <util/thread/threading.hpp>

#include <condition_variable>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

using namespace llarp;
using namespace llarp::thread;

using LockGuard = std::unique_lock< std::mutex >;

class PoolArgs
{
 public:
  std::mutex& mutex;
  std::condition_variable& start;
  std::condition_variable& stop;
  volatile size_t count;
  volatile size_t startSignal;
  volatile size_t stopSignal;
};

class BarrierArgs
{
 public:
  util::Barrier& startBarrier;
  util::Barrier& stopBarrier;

  std::atomic_size_t count;
};

class BasicWorkArgs
{
 public:
  std::atomic_size_t count;
};

void
simpleFunction(PoolArgs& args)
{
  LockGuard lock(args.mutex);
  ++args.count;
  ++args.startSignal;
  args.start.notify_one();

  args.stop.wait(lock, [&]() { return args.stopSignal; });
}

void
incrementFunction(PoolArgs& args)
{
  LockGuard lock(args.mutex);
  ++args.count;
  ++args.startSignal;
  args.start.notify_one();
}

void
barrierFunction(BarrierArgs& args)
{
  args.startBarrier.Block();
  args.count++;
  args.stopBarrier.Block();
}

void
basicWork(BasicWorkArgs& args)
{
  args.count++;
}

void
recurse(util::Barrier& barrier, std::atomic_size_t& counter, ThreadPool& pool,
        size_t depthLimit)
{
  ASSERT_LE(0u, counter);
  ASSERT_GT(depthLimit, counter);

  if(++counter != depthLimit)
  {
    ASSERT_TRUE(
        pool.addJob(std::bind(recurse, std::ref(barrier), std::ref(counter),
                              std::ref(pool), depthLimit)));
  }

  barrier.Block();
}

class DestructiveObject
{
 private:
  util::Barrier& barrier;
  ThreadPool& pool;

 public:
  DestructiveObject(util::Barrier& b, ThreadPool& p) : barrier(b), pool(p)
  {
  }

  ~DestructiveObject()
  {
    auto job = std::bind(&util::Barrier::Block, &barrier);
    pool.addJob(job);
  }
};

void
destructiveJob(DestructiveObject* obj)
{
  delete obj;
}

TEST(TestThreadPool, breathing)
{
  static constexpr size_t threads  = 10;
  static constexpr size_t capacity = 50;

  ThreadPool pool(threads, capacity, "breathing");

  ASSERT_EQ(0u, pool.startedThreadCount());
  ASSERT_EQ(capacity, pool.capacity());
  ASSERT_EQ(0u, pool.jobCount());

  ASSERT_TRUE(pool.start());

  ASSERT_EQ(threads, pool.startedThreadCount());
  ASSERT_EQ(capacity, pool.capacity());
  ASSERT_EQ(0u, pool.jobCount());

  pool.drain();
}

struct AccessorsData
{
  size_t threads;
  size_t capacity;
};

std::ostream&
operator<<(std::ostream& os, AccessorsData d)
{
  os << "[ threads = " << d.threads << " capacity = " << d.capacity << " ]";
  return os;
}

class Accessors : public ::testing::TestWithParam< AccessorsData >
{
};

TEST_P(Accessors, accessors)
{
  auto d = GetParam();

  ThreadPool pool(d.threads, d.capacity, "accessors");

  ASSERT_EQ(d.threads, pool.threadCount());
  ASSERT_EQ(d.capacity, pool.capacity());
  ASSERT_EQ(0u, pool.startedThreadCount());
}

static const AccessorsData accessorsData[] = {
    {10, 50}, {1, 1}, {50, 100}, {2, 22}, {100, 200}};

INSTANTIATE_TEST_SUITE_P(TestThreadPool, Accessors,
                         ::testing::ValuesIn(accessorsData));

struct ClosingData
{
  size_t threads;
  size_t capacity;
};

std::ostream&
operator<<(std::ostream& os, ClosingData d)
{
  os << "[ threads = " << d.threads << " capacity = " << d.capacity << " ]";
  return os;
}

class Closing : public ::testing::TestWithParam< ClosingData >
{
};

TEST_P(Closing, drain)
{
  auto d = GetParam();

  std::mutex mutex;
  std::condition_variable start;
  std::condition_variable stop;

  PoolArgs args{mutex, start, stop, 0, 0, 0};

  ThreadPool pool(d.threads, d.capacity, "drain");

  ASSERT_EQ(d.threads, pool.threadCount());
  ASSERT_EQ(d.capacity, pool.capacity());
  ASSERT_EQ(0u, pool.startedThreadCount());

  auto simpleJob = std::bind(simpleFunction, std::ref(args));

  ASSERT_FALSE(pool.addJob(simpleJob));

  ASSERT_TRUE(pool.start());
  ASSERT_EQ(0u, pool.jobCount());

  LockGuard lock(mutex);

  for(size_t i = 0; i < d.threads; ++i)
  {
    args.startSignal = 0;
    args.stopSignal  = 0;
    ASSERT_TRUE(pool.addJob(simpleJob));

    start.wait(lock, [&]() { return args.startSignal; });
  }

  args.stopSignal++;

  lock.unlock();

  stop.notify_all();

  pool.drain();

  ASSERT_EQ(d.threads, pool.startedThreadCount());
  ASSERT_EQ(0u, pool.jobCount());
}

TEST_P(Closing, stop)
{
  auto d = GetParam();

  ThreadPool pool(d.threads, d.capacity, "stop");

  std::mutex mutex;
  std::condition_variable start;
  std::condition_variable stop;

  PoolArgs args{mutex, start, stop, 0, 0, 0};

  ASSERT_EQ(d.threads, pool.threadCount());
  ASSERT_EQ(d.capacity, pool.capacity());
  ASSERT_EQ(0u, pool.startedThreadCount());

  auto simpleJob = std::bind(simpleFunction, std::ref(args));

  ASSERT_FALSE(pool.addJob(simpleJob));

  ASSERT_TRUE(pool.start());
  ASSERT_EQ(0u, pool.jobCount());

  LockGuard lock(mutex);

  for(size_t i = 0; i < d.capacity; ++i)
  {
    args.startSignal = 0;
    args.stopSignal  = 0;
    ASSERT_TRUE(pool.addJob(simpleJob));

    while(i < d.threads && !args.startSignal)
    {
      start.wait(lock);
    }
  }

  args.stopSignal++;

  lock.unlock();

  stop.notify_all();

  pool.stop();

  ASSERT_EQ(d.capacity, args.count);
  ASSERT_EQ(0u, pool.startedThreadCount());
  ASSERT_EQ(0u, pool.activeThreadCount());
  ASSERT_EQ(0u, pool.jobCount());
}

TEST_P(Closing, shutdown)
{
  auto d = GetParam();

  ThreadPool pool(d.threads, d.capacity, "shutdown");

  std::mutex mutex;
  std::condition_variable start;
  std::condition_variable stop;

  PoolArgs args{mutex, start, stop, 0, 0, 0};

  ASSERT_EQ(d.threads, pool.threadCount());
  ASSERT_EQ(d.capacity, pool.capacity());
  ASSERT_EQ(0u, pool.startedThreadCount());

  auto simpleJob = std::bind(simpleFunction, std::ref(args));

  ASSERT_FALSE(pool.addJob(simpleJob));

  ASSERT_TRUE(pool.start());
  ASSERT_EQ(0u, pool.jobCount());

  LockGuard lock(mutex);

  for(size_t i = 0; i < d.capacity; ++i)
  {
    args.startSignal = 0;
    args.stopSignal  = 0;
    ASSERT_TRUE(pool.addJob(simpleJob));

    while(i < d.threads && !args.startSignal)
    {
      start.wait(lock);
    }
  }

  ASSERT_EQ(d.threads, pool.startedThreadCount());
  ASSERT_EQ(d.capacity - d.threads, pool.jobCount());

  auto incrementJob = std::bind(incrementFunction, std::ref(args));

  for(size_t i = 0; i < d.threads; ++i)
  {
    ASSERT_TRUE(pool.addJob(incrementJob));
  }

  args.stopSignal++;
  stop.notify_all();

  lock.unlock();

  pool.shutdown();

  ASSERT_EQ(0u, pool.startedThreadCount());
  ASSERT_EQ(0u, pool.activeThreadCount());
  ASSERT_EQ(0u, pool.jobCount());
}

ClosingData closingData[] = {{1, 1},   {2, 2},   {10, 10},
                             {10, 50}, {50, 75}, {25, 80}};

INSTANTIATE_TEST_SUITE_P(TestThreadPool, Closing,
                         ::testing::ValuesIn(closingData));

struct TryAddData
{
  size_t threads;
  size_t capacity;
};

std::ostream&
operator<<(std::ostream& os, TryAddData d)
{
  os << "[ threads = " << d.threads << " capacity = " << d.capacity << " ]";
  return os;
}

class TryAdd : public ::testing::TestWithParam< TryAddData >
{
};

TEST_P(TryAdd, noblocking)
{
  // Verify that tryAdd does not block.
  // Fill the queue, then verify `tryAddJob` does not block.
  auto d = GetParam();

  ThreadPool pool(d.threads, d.capacity, "noblocking");

  util::Barrier startBarrier(d.threads + 1);
  util::Barrier stopBarrier(d.threads + 1);

  BarrierArgs args{startBarrier, stopBarrier, {0}};

  auto simpleJob = std::bind(barrierFunction, std::ref(args));

  ASSERT_FALSE(pool.tryAddJob(simpleJob));

  ASSERT_TRUE(pool.start());

  for(size_t i = 0; i < d.threads; ++i)
  {
    ASSERT_TRUE(pool.tryAddJob(simpleJob));
  }

  // Wait for everything to start.
  startBarrier.Block();

  // and that we emptied the queue.
  ASSERT_EQ(0u, pool.jobCount());

  BasicWorkArgs basicWorkArgs = {{0}};

  auto workJob = std::bind(basicWork, std::ref(basicWorkArgs));

  for(size_t i = 0; i < d.capacity; ++i)
  {
    ASSERT_TRUE(pool.tryAddJob(workJob));
  }

  // queue should now be full
  ASSERT_FALSE(pool.tryAddJob(workJob));

  // and finish
  stopBarrier.Block();
}

TEST(TestThreadPool, recurseJob)
{
  // Verify we can enqueue a job onto the threadpool from a thread which is
  // currently executing a threadpool job.

  static constexpr size_t threads  = 10;
  static constexpr size_t depth    = 10;
  static constexpr size_t capacity = 100;

  util::Barrier barrier(threads + 1);
  std::atomic_size_t counter{0};

  ThreadPool pool(threads, capacity, "recurse");

  pool.start();

  ASSERT_TRUE(pool.addJob(std::bind(recurse, std::ref(barrier),
                                    std::ref(counter), std::ref(pool), depth)));

  barrier.Block();
  ASSERT_EQ(depth, counter);
}

TEST(TestThreadPool, destructors)
{
  // Verify that functors have their destructors called outside of threadpool
  // locks.

  static constexpr size_t threads  = 1;
  static constexpr size_t capacity = 100;

  ThreadPool pool(threads, capacity, "destructors");

  pool.start();

  util::Barrier barrier(threads + 1);

  {
    DestructiveObject* obj = new DestructiveObject(barrier, pool);
    ASSERT_TRUE(pool.addJob(std::bind(destructiveJob, obj)));
  }

  barrier.Block();
}
