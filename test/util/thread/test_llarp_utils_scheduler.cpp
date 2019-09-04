#include <util/thread/scheduler.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;
using thread::Scheduler;
using thread::Tardis;

TEST(SchedulerTest, smoke)
{
  Scheduler scheduler;
  ASSERT_TRUE(scheduler.start());
  scheduler.stop();
}

struct TestCallback
{
  std::atomic_size_t m_startCount;
  std::atomic_size_t m_execCount;

  absl::Duration executeTime;

  TestCallback() : m_startCount(0), m_execCount(0), executeTime()
  {
  }

  void
  callback()
  {
    m_startCount++;

    if(executeTime != absl::Duration())
    {
      std::this_thread::sleep_for(absl::ToChronoSeconds(executeTime));
    }

    m_execCount++;
  }

  void
  waitFor(absl::Duration duration, size_t attemptCount,
          size_t executeCount) const
  {
    for(size_t i = 0; i < attemptCount; ++i)
    {
      if(executeCount + 1 <= m_execCount)
      {
        return;
      }
      std::this_thread::sleep_until(absl::ToChronoTime(absl::Now() + duration));
      std::this_thread::yield();
    }
  }
};

TEST(SchedulerTest, fakeTime)
{
  // Just test we can mock out Time itself
  Scheduler scheduler;
  Tardis time{scheduler};

  absl::Time now = time.now();

  TestCallback callback1, callback2;

  Scheduler::Handle handle = scheduler.schedule(
      now + absl::Seconds(30), std::bind(&TestCallback::callback, &callback1));

  ASSERT_NE(Scheduler::INVALID_HANDLE, handle);

  handle = scheduler.scheduleRepeat(
      absl::Seconds(60), std::bind(&TestCallback::callback, &callback2));

  ASSERT_NE(Scheduler::INVALID_HANDLE, handle);

  scheduler.start();

  time.advanceTime(absl::Seconds(35));
  ASSERT_EQ(time.now(), now + absl::Seconds(35));

  callback1.waitFor(absl::Milliseconds(10), 100, 0);

  ASSERT_EQ(1u, callback1.m_execCount);
  ASSERT_EQ(0u, callback2.m_execCount);

  // jump forward another 30 seconds, the repeat event should kick off
  time.advanceTime(absl::Seconds(30));
  ASSERT_EQ(time.now(), now + absl::Seconds(65));

  callback2.waitFor(absl::Milliseconds(10), 100, 0);

  ASSERT_EQ(1u, callback1.m_execCount);
  ASSERT_EQ(1u, callback2.m_execCount);

  // jump forward another minute, the repeat event should have run again
  time.advanceTime(absl::Seconds(60));

  callback2.waitFor(absl::Milliseconds(10), 100, 1);

  ASSERT_EQ(1u, callback1.m_execCount);
  ASSERT_EQ(2u, callback2.m_execCount);

  scheduler.stop();
}

TEST(SchedulerTest, func1)
{
  Scheduler scheduler;
  scheduler.start();

  TestCallback callback1, callback2;

  absl::Time now = absl::Now();
  scheduler.scheduleRepeat(absl::Milliseconds(30),
                           std::bind(&TestCallback::callback, &callback1));

  scheduler.schedule(now + absl::Milliseconds(60),
                     std::bind(&TestCallback::callback, &callback2));

  std::this_thread::yield();
  std::this_thread::sleep_for(absl::ToChronoSeconds(absl::Milliseconds(40)));

  callback1.waitFor(absl::Milliseconds(10), 100, 0);

  scheduler.stop();

  absl::Duration elapsed = absl::Now() - now;

  size_t count1 = callback1.m_execCount;
  size_t count2 = callback2.m_execCount;

  if(elapsed < absl::Milliseconds(60))
  {
    ASSERT_EQ(1u, count1);
    ASSERT_EQ(0u, count2);
  }
  else
  {
    ASSERT_LE(1u, count1);
  }

  callback1.waitFor(absl::Milliseconds(10), 100, 0);

  size_t count = callback1.m_execCount;
  ASSERT_EQ(count1, count);
  count = callback2.m_execCount;
  ASSERT_EQ(count2, count);

  if(count2 == 0)
  {
    // callback2 not executed
    scheduler.start();
    std::this_thread::yield();
    std::this_thread::sleep_for(absl::ToChronoSeconds(absl::Milliseconds(40)));
    callback2.waitFor(absl::Milliseconds(10), 100, count2);
    count = callback2.m_execCount;
    ASSERT_LE(count2 + 1, count);
  }
  else
  {
    ASSERT_LT(absl::Milliseconds(60), elapsed);
  }
}

TEST(SchedulerTest, cancelAllRepeats)
{
  Scheduler scheduler;
  scheduler.start();

  TestCallback callback1, callback2;

  const Scheduler::Handle handle1 = scheduler.scheduleRepeat(
      absl::Milliseconds(30), std::bind(&TestCallback::callback, &callback1));

  const Scheduler::Handle handle2 = scheduler.scheduleRepeat(
      absl::Milliseconds(30), std::bind(&TestCallback::callback, &callback2));

  scheduler.cancelAllRepeats();
  ASSERT_FALSE(scheduler.cancelRepeat(handle1));
  ASSERT_FALSE(scheduler.cancelRepeat(handle2));

  const size_t count1 = callback1.m_execCount;
  const size_t count2 = callback2.m_execCount;
  std::this_thread::yield();
  std::this_thread::sleep_for(absl::ToChronoSeconds(absl::Milliseconds(100)));
  size_t count = callback1.m_execCount;

  ASSERT_EQ(count1, count);
  count = callback2.m_execCount;
  ASSERT_EQ(count2, count);

  scheduler.stop();
}
