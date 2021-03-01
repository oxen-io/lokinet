#include <util/thread/queue.hpp>
#include <util/thread/threading.hpp>
#include <util/thread/barrier.hpp>

#include <array>
#include <condition_variable>
#include <functional>
#include <thread>

#include <catch2/catch.hpp>

using namespace llarp;
using namespace llarp::thread;

using namespace std::literals;

using LockGuard = std::unique_lock<std::mutex>;

class Element
{
 private:
  double data;
  bool shouldStop;

 public:
  Element(double d, bool _stop = false) : data(d), shouldStop(_stop)
  {}

  double
  val() const
  {
    return data;
  }

  bool
  stop() const
  {
    return shouldStop;
  }
};

bool
operator==(const Element& lhs, const Element& rhs)
{
  return lhs.val() == rhs.val();
}

using ObjQueue = Queue<Element>;

class Args
{
 public:
  std::condition_variable startCond;
  std::condition_variable runCond;
  std::mutex mutex;
  std::condition_variable cv;

  ObjQueue queue;

  // Use volatile over atomic int in order to verify the thread safety.
  // If we used atomics here, we would introduce new potential synchronisation
  // points.
  volatile size_t iterations;
  volatile size_t count;
  volatile size_t startSignal;
  volatile size_t runSignal;
  volatile size_t endSignal;

  Args(size_t _iterations, size_t size = 20 * 1000)
      : queue(size), iterations(_iterations), count(0), startSignal(0), runSignal(0), endSignal(0)
  {}

  bool
  signal() const
  {
    return !!runSignal;
  }
};

void
popFrontTester(Args& args)
{
  {
    LockGuard lock(args.mutex);
    args.count++;
    args.cv.wait(lock, [&] { return args.signal(); });
  }

  for (;;)
  {
    Element e = args.queue.popFront();
    if (e.stop())
    {
      break;
    }
  }
}

void
pushBackTester(Args& args)
{
  {
    LockGuard lock(args.mutex);
    args.count++;
    args.cv.wait(lock, [&] { return args.signal(); });
  }

  for (size_t i = 0; i < args.iterations; ++i)
  {
    Element e{static_cast<double>(i)};
    args.queue.pushBack(e);
  }

  args.queue.pushBack(Element{0, true});
}

void
abaThread(char* firstValue, char* lastValue, Queue<char*>& queue, util::Barrier& barrier)
{
  barrier.Block();

  for (char* val = firstValue; val <= lastValue; ++val)
  {
    queue.pushBack(val);
  }
}

struct Exception : public std::exception
{};

struct ExceptionTester
{
  static std::atomic<std::thread::id> throwFrom;

  void
  test()
  {
    if (throwFrom != std::thread::id() && std::this_thread::get_id() == throwFrom)
    {
      throw Exception();
    }
  }

  ExceptionTester()
  {}

  ExceptionTester(const ExceptionTester&)
  {
    test();
  }

  ExceptionTester&
  operator=(const ExceptionTester&)
  {
    test();
    return *this;
  }
};

std::atomic<std::thread::id> ExceptionTester::throwFrom = {std::thread::id()};

void
sleepNWait(std::chrono::microseconds microseconds, util::Barrier& barrier)
{
  std::this_thread::sleep_for(microseconds);
  barrier.Block();
}

void
exceptionProducer(
    Queue<ExceptionTester>& queue, util::Semaphore& semaphore, std::atomic_size_t& caught)
{
  static constexpr size_t iterations = 3;

  for (size_t i = 0; i < iterations; ++i)
  {
    try
    {
      queue.pushBack(ExceptionTester());
    }
    catch (const Exception&)
    {
      ++caught;
    }

    semaphore.notify();
  }
}

struct MoveTester
{
  bool moved;
  size_t& moveCounter;
  size_t value;

  explicit MoveTester(size_t& counter, size_t val) : moved(false), moveCounter(counter), value(val)
  {}

  explicit MoveTester(const MoveTester& rhs) = delete;

  MoveTester&
  operator=(const MoveTester& rhs) = delete;

  MoveTester(MoveTester&& rhs) : moved(false), moveCounter(rhs.moveCounter), value(rhs.value)
  {
    rhs.moved = true;
    moveCounter++;
  }

  MoveTester&
  operator=(MoveTester&& rhs)
  {
    value = rhs.value;
    rhs.moved = true;
    moveCounter = rhs.moveCounter;

    moveCounter++;

    return *this;
  }
};

TEST_CASE("single")
{
  ObjQueue queue(1u);

  REQUIRE(0u == queue.size());
  REQUIRE(1u == queue.capacity());
}

TEST_CASE("breathing")
{
  static constexpr size_t DEFAULT_CAP = 10 * 1000;

  ObjQueue queue(DEFAULT_CAP);

  REQUIRE(0u == queue.size());
  REQUIRE(DEFAULT_CAP == queue.capacity());

  Element e1(1.0);
  Element e2(2.0);
  Element e3(3.0);

  queue.pushBack(e1);
  queue.pushBack(e2);
  queue.pushBack(e3);

  Element p1 = queue.popFront();
  Element p2 = queue.popFront();
  Element p3 = queue.popFront();

  REQUIRE(e1 == p1);
  REQUIRE(e2 == p2);
  REQUIRE(e3 == p3);
}

TEST_CASE("Single producer many consumer")
{
  static constexpr size_t iterations = 100 * 1000;
  static constexpr size_t numThreads = 5;

  std::array<std::thread, numThreads> threads;

  Args args{iterations};

  {
    LockGuard lock(args.mutex);

    for (size_t i = 0; i < threads.size(); ++i)
    {
      threads[i] = std::thread(std::bind(&popFrontTester, std::ref(args)));
      args.cv.wait(lock, [&] { return args.count != i + 1; });
    }

    args.runSignal++;
  }

  for (size_t i = 0; i < iterations; ++i)
  {
    Element e{static_cast<double>(i)};
    args.queue.pushBack(e);
  }

  for (size_t i = 0; i < numThreads; ++i)
  {
    Element e{0.0, true};
    args.queue.pushBack(e);
  }

  for (size_t i = 0; i < numThreads; ++i)
  {
    threads[i].join();
  }

  REQUIRE(0u == args.queue.size());
}

TEST_CASE("Many producer many consumer")
{
  static constexpr size_t iterations = 100 * 1000;
  static constexpr size_t numThreads = 5;

  std::array<std::thread, numThreads * 2> threads;

  Args args{iterations};

  {
    LockGuard lock(args.mutex);

    for (size_t i = 0; i < numThreads; ++i)
    {
      threads[i] = std::thread(std::bind(&popFrontTester, std::ref(args)));
      args.cv.wait(lock, [&] { return args.count != i + 1; });
    }

    for (size_t i = 0; i < numThreads; ++i)
    {
      threads[i + numThreads] = std::thread(std::bind(&pushBackTester, std::ref(args)));
      args.cv.wait(lock, [&] { return args.count != numThreads + i + 1; });
    }

    args.runSignal++;
  }

  for (auto& thread : threads)
  {
    thread.join();
  }

  REQUIRE(0u == args.queue.size());
}

TEST_CASE("ABA empty")
{
  // Verify we avoid the ABA problem, where multiple threads try to push an
  // object to the same "empty" position in the queue.

  static constexpr size_t numThreads = 50;
  static constexpr size_t numValues = 6;
  static constexpr size_t numIterations = 1000;
  static constexpr size_t numEntries = numThreads * numValues;

  char block[numEntries];

  for (size_t i = 0; i < numIterations; ++i)
  {
    util::Barrier barrier{numThreads + 1};

    Queue<char*> queue{numEntries + 1};

    std::array<std::thread, numThreads + 1> threads;

    char* nextValue[numThreads];
    char* lastValue[numThreads];

    for (size_t j = 0; j < numThreads; ++j)
    {
      nextValue[j] = block + (numValues * j);
      lastValue[j] = block + (numValues * (j + 1)) - 1;

      threads[j] =
          std::thread([&, n = nextValue[j], l = lastValue[j]] { abaThread(n, l, queue, barrier); });
    }

    threads[numThreads] = std::thread([&] {
      std::this_thread::sleep_for(100us);
      barrier.Block();
    });

    for (size_t j = 0; j < numEntries; ++j)
    {
      char* val = queue.popFront();

      size_t k = 0;

      for (k = 0; k < numThreads; ++k)
      {
        if (val == nextValue[k])
        {
          nextValue[k] += (val == lastValue[k] ? 0 : 1);
          REQUIRE(nextValue[k] <= lastValue[k]);
          break;
        }
      }

      REQUIRE(k < numThreads);
    }

    for (auto& thread : threads)
    {
      thread.join();
    }

    REQUIRE(0u == queue.size());
  }
}

TEST_CASE("Generation count")
{
  // Verify functionality after running through a full cycle (and invoking the
  // generation rollover logic).
  // For a queue of size 3, this is currently 508 cycles, implying we need to go
  // through at least 3048 objects (3 * 508 * 2) to trigger this logic twice.
  static constexpr size_t numThreads = 6;
  static constexpr size_t queueSize = 3;
  static constexpr size_t numEntries = 3060;
  static constexpr size_t numValues = numEntries / numThreads;

  char block[numEntries];

  util::Barrier barrier{numThreads + 1};

  Queue<char*> queue{queueSize};

  std::array<std::thread, numThreads + 1> threads;

  char* nextValue[numThreads];
  char* lastValue[numThreads];

  for (size_t j = 0; j < numThreads; ++j)
  {
    nextValue[j] = block + (numValues * j);
    lastValue[j] = block + (numValues * (j + 1)) - 1;

    threads[j] =
        std::thread([&, n = nextValue[j], l = lastValue[j]] { abaThread(n, l, queue, barrier); });
  }

  threads[numThreads] = std::thread([&] {
    std::this_thread::sleep_for(100ms);
    barrier.Block();
  });

  for (size_t j = 0; j < numEntries; ++j)
  {
    char* val = queue.popFront();

    size_t k = 0;

    for (k = 0; k < numThreads; ++k)
    {
      if (val == nextValue[k])
      {
        nextValue[k] += (val == lastValue[k] ? 0 : 1);
        REQUIRE(nextValue[k] <= lastValue[k]);
        break;
      }
    }

    REQUIRE(k < numThreads);
  }

  for (auto& thread : threads)
  {
    thread.join();
  }

  REQUIRE(0u == queue.size());
}

TEST_CASE("Basic exception safety")
{
  ExceptionTester::throwFrom = std::this_thread::get_id();

  Queue<ExceptionTester> queue{1};

  REQUIRE_THROWS_AS(queue.pushBack(ExceptionTester()), Exception);

  ExceptionTester::throwFrom = std::thread::id();
}

TEST_CASE("Exception safety")
{
  ExceptionTester::throwFrom = std::thread::id();
  static constexpr size_t queueSize = 3;

  Queue<ExceptionTester> queue{queueSize};

  REQUIRE(QueueReturn::Success == queue.pushBack(ExceptionTester()));
  REQUIRE(QueueReturn::Success == queue.pushBack(ExceptionTester()));
  REQUIRE(QueueReturn::Success == queue.pushBack(ExceptionTester()));
  REQUIRE(QueueReturn::Success != queue.tryPushBack(ExceptionTester()));

  util::Semaphore semaphore{0};

  std::atomic_size_t caught = {0};

  std::thread producer(
      std::bind(&exceptionProducer, std::ref(queue), std::ref(semaphore), std::ref(caught)));

  ExceptionTester::throwFrom = std::this_thread::get_id();

  REQUIRE_THROWS_AS(queue.popFront(), Exception);

  using namespace std::literals;
  // Now the queue is not full, and the producer thread can start adding items.
  REQUIRE(semaphore.waitFor(1s));

  REQUIRE(queueSize == queue.size());

  REQUIRE_THROWS_AS(queue.popFront(), Exception);

  // Now the queue is not full, and the producer thread can start adding items.
  REQUIRE(semaphore.waitFor(1s));

  REQUIRE(queueSize == queue.size());

  // Pushing into the queue with exception empties the queue.
  ExceptionTester::throwFrom = producer.get_id();

  // pop an item to unblock the pusher
  (void)queue.popFront();

  REQUIRE(semaphore.waitFor(1s));

  REQUIRE(1u == caught);

  REQUIRE(0u == queue.size());
  REQUIRE(queue.empty());

  // after throwing, the queue works fine.

  REQUIRE(QueueReturn::Success == queue.pushBack(ExceptionTester()));
  REQUIRE(QueueReturn::Success == queue.pushBack(ExceptionTester()));
  REQUIRE(QueueReturn::Success == queue.pushBack(ExceptionTester()));
  REQUIRE(QueueReturn::Success != queue.tryPushBack(ExceptionTester()));

  ExceptionTester::throwFrom = std::thread::id();

  producer.join();
}

TEST_CASE("Move it")
{
  static constexpr size_t queueSize = 40;

  Queue<MoveTester> queue{queueSize};

  size_t counter = 0;

  queue.pushBack(MoveTester{counter, 0});

  REQUIRE(1u == counter);

  MoveTester tester2(counter, 2);
  queue.pushBack(std::move(tester2));

  REQUIRE(tester2.moved);
  REQUIRE(2u == counter);

  REQUIRE(QueueReturn::Success == queue.tryPushBack(MoveTester{counter, 3}));
  REQUIRE(3u == counter);

  MoveTester tester4(counter, 4);
  REQUIRE(QueueReturn::Success == queue.tryPushBack(std::move(tester4)));

  REQUIRE(tester4.moved);
  REQUIRE(4u == counter);

  MoveTester popped = queue.popFront();
  (void)popped;

  REQUIRE(5u == counter);

  std::optional<MoveTester> optPopped = queue.tryPopFront();

  REQUIRE(optPopped);

  // Moved twice here to construct the optional.
  REQUIRE(6u == counter);
}
