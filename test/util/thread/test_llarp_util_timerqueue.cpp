#include <util/thread/timerqueue.hpp>

#include <thread>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using CharQueue = llarp::thread::TimerQueue< const char* >;
using CharItem  = llarp::thread::TimerQueueItem< const char* >;

TEST(TimerQueue, smoke)
{
  CharQueue queue;
  const absl::Time TA = absl::Time();
  const absl::Time TB = TA + absl::Seconds(1);
  const absl::Time TC = TB + absl::Seconds(1);
  const absl::Time TD = TC + absl::Seconds(1);
  const absl::Time TE = TD + absl::Seconds(1);

  const char* VA = "hello";
  const char* VB = "world,";
  const char* VC = "how";
  const char* VD = "are";
  const char* VE = "you";

  int HA = queue.add(TA, VA);
  int HB = queue.add(TB, VB);
  int HC = queue.add(TC, VC);
  int HD = queue.add(TD, VD);
  int HE = queue.add(TE, VE);

  CharItem tItem;
  absl::Time newMinTime;
  size_t newSize;

  ASSERT_TRUE(queue.popFront(&tItem));
  ASSERT_EQ(VA, tItem.value());
  ASSERT_EQ(TA, tItem.time());
  ASSERT_EQ(HA, tItem.handle());

  ASSERT_TRUE(queue.popFront(&tItem, &newSize, &newMinTime));
  ASSERT_EQ(3, newSize);
  ASSERT_EQ(TC, newMinTime);
  ASSERT_EQ(TB, tItem.time());
  ASSERT_EQ(VB, tItem.value());
  ASSERT_EQ(HB, tItem.handle());

  std::vector< CharItem > a1;

  queue.popLess(TD, &a1, &newSize, &newMinTime);
  ASSERT_EQ(2, a1.size());
  ASSERT_EQ(1, newSize);
  ASSERT_EQ(TE, newMinTime);
  ASSERT_EQ(TC, a1[0].time());
  ASSERT_EQ(VC, a1[0].value());
  ASSERT_EQ(HC, a1[0].handle());
  ASSERT_EQ(TD, a1[1].time());
  ASSERT_EQ(VD, a1[1].value());
  ASSERT_EQ(HD, a1[1].handle());

  std::vector< CharItem > a2;

  queue.popLess(TD, &a2, &newSize, &newMinTime);
  ASSERT_EQ(0, a2.size());
  ASSERT_EQ(1, newSize);
  ASSERT_EQ(TE, newMinTime);

  std::vector< CharItem > a3;

  queue.popLess(TE, &a3, &newSize, &newMinTime);
  ASSERT_EQ(1, a3.size());
  ASSERT_EQ(0, newSize);
  ASSERT_EQ(TE, a3[0].time());
  ASSERT_EQ(VE, a3[0].value());
  ASSERT_EQ(HE, a3[0].handle());
}

TEST(TimerQueue, KeySmoke)
{
  CharQueue x1;
  const absl::Time TA = absl::Time();
  const absl::Time TB = TA + absl::Seconds(1);
  const absl::Time TC = TB + absl::Seconds(1);
  const absl::Time TD = TC + absl::Seconds(1);
  const absl::Time TE = TD + absl::Seconds(1);

  const char* VA = "hello";
  const char* VB = "world,";
  const char* VC = "how";
  const char* VD = "are";
  const char* VE = "you";

  typedef CharQueue::Key Key;

  const Key KA = Key(&TA);
  const Key KB = Key(&TB);
  const Key KC = Key(382);
  const Key KD = Key(123);
  const Key KE = Key(&VE);

  int HA = x1.add(TA, VA, KA);
  int HB = x1.add(TB, VB, KB);
  int HC = x1.add(TC, VC, KC);
  int HD = x1.add(TD, VD, KD);
  int HE = x1.add(TE, VE, KE);

  ASSERT_FALSE(x1.remove(HA, KB));
  ASSERT_TRUE(x1.isValid(HA, KA));
  ASSERT_FALSE(x1.update(HC, KD, TE));

  CharItem tItem;
  absl::Time newMinTime;
  size_t newSize;

  ASSERT_TRUE(x1.popFront(&tItem));
  ASSERT_EQ(VA, tItem.value());
  ASSERT_EQ(TA, tItem.time());
  ASSERT_EQ(HA, tItem.handle());
  ASSERT_EQ(KA, tItem.key());

  ASSERT_TRUE(x1.popFront(&tItem, &newSize, &newMinTime));
  ASSERT_EQ(3, newSize);
  ASSERT_EQ(TC, newMinTime);
  ASSERT_EQ(TB, tItem.time());
  ASSERT_EQ(VB, tItem.value());
  ASSERT_EQ(HB, tItem.handle());
  ASSERT_EQ(KB, tItem.key());

  std::vector< CharItem > a1;

  x1.popLess(TD, &a1, &newSize, &newMinTime);
  ASSERT_EQ(2, a1.size());
  ASSERT_EQ(1, newSize);
  ASSERT_EQ(TE, newMinTime);
  ASSERT_EQ(TC, a1[0].time());
  ASSERT_EQ(VC, a1[0].value());
  ASSERT_EQ(HC, a1[0].handle());
  ASSERT_EQ(KC, a1[0].key());
  ASSERT_EQ(TD, a1[1].time());
  ASSERT_EQ(VD, a1[1].value());
  ASSERT_EQ(HD, a1[1].handle());
  ASSERT_EQ(KD, a1[1].key());

  std::vector< CharItem > a2;

  x1.popLess(TD, &a2, &newSize, &newMinTime);
  ASSERT_EQ(0, a2.size());
  ASSERT_EQ(1, newSize);
  ASSERT_EQ(TE, newMinTime);

  std::vector< CharItem > a3;

  x1.popLess(TE, &a3, &newSize, &newMinTime);
  ASSERT_EQ(1, a3.size());
  ASSERT_EQ(0, newSize);
  ASSERT_EQ(TE, a3[0].time());
  ASSERT_EQ(VE, a3[0].value());
  ASSERT_EQ(HE, a3[0].handle());
  ASSERT_EQ(KE, a3[0].key());
}

TEST(TimerQueue, Update)
{
  const char VA[] = "A";
  const char VB[] = "B";
  const char VC[] = "C";
  const char VD[] = "D";
  const char VE[] = "E";

  // clang-format off
  static const struct
  {
    int m_secs;
    int m_nsecs;
    const char* m_value;
    int m_updsecs;
    int m_updnsecs;
    bool m_isNewTop;
  } VALUES[] = {
      {2, 1000000, VA, 0, 1000000, false},
      {2, 1000000, VB, 3, 1000000, false},
      {2, 1000000, VC, 0, 4000, false},
      {2, 1000001, VB, 0, 3999, true},
      {1, 9999998, VC, 4, 9999998, false},
      {1, 9999999, VD, 0, 0, true},
      {0, 4000, VE, 10, 4000, false}};
  // clang-format on

  static const int POP_ORDER[] = {5, 3, 2, 0, 1, 4, 6};

  const int NUM_VALUES = sizeof VALUES / sizeof *VALUES;
  int handles[NUM_VALUES];

  CharQueue queue;

  {
    CharItem item;
    ASSERT_FALSE(queue.popFront(&item));
  }

  for(int i = 0; i < NUM_VALUES; ++i)
  {
    const char* VAL = VALUES[i].m_value;
    const int SECS  = VALUES[i].m_secs;
    const int NSECS = VALUES[i].m_nsecs;
    absl::Time TIME =
        absl::Time() + absl::Seconds(SECS) + absl::Nanoseconds(NSECS);

    handles[i] = queue.add(TIME, VAL);

    ASSERT_EQ(i + 1, queue.size());
    ASSERT_TRUE(queue.isValid(handles[i]));
  }

  for(int i = 0; i < NUM_VALUES; ++i)
  {
    const int UPDSECS    = VALUES[i].m_updsecs;
    const bool EXPNEWTOP = VALUES[i].m_isNewTop;
    const int UPDNSECS   = VALUES[i].m_updnsecs;
    absl::Time UPDTIME =
        absl::Time() + absl::Seconds(UPDSECS) + absl::Nanoseconds(UPDNSECS);

    bool isNewTop;

    CharItem item;
    ASSERT_TRUE(queue.isValid(handles[i])) << i;
    ASSERT_TRUE(queue.update(handles[i], UPDTIME, &isNewTop)) << i;
    EXPECT_EQ(EXPNEWTOP, isNewTop) << i;
    ASSERT_TRUE(queue.isValid(handles[i])) << i;
  }

  for(int i = 0; i < NUM_VALUES; ++i)
  {
    const int I        = POP_ORDER[i];
    const char* EXPVAL = VALUES[I].m_value;
    const int EXPSECS  = VALUES[I].m_updsecs;
    const int EXPNSECS = VALUES[I].m_updnsecs;
    absl::Time EXPTIME =
        absl::Time() + absl::Seconds(EXPSECS) + absl::Nanoseconds(EXPNSECS);

    CharItem item;
    ASSERT_TRUE(queue.isValid(handles[I]));
    ASSERT_TRUE(queue.popFront(&item));
    ASSERT_EQ(EXPTIME, item.time());
    ASSERT_EQ(EXPVAL, item.value());
    ASSERT_FALSE(queue.isValid(handles[I]));
  }
}

TEST(TimerQueue, ThreadSafety)
{
  using Data        = std::string;
  using StringQueue = llarp::thread::TimerQueue< std::string >;
  using StringItem  = llarp::thread::TimerQueueItem< std::string >;

  using Info = std::pair< int, std::vector< StringItem >* >;

  static constexpr size_t NUM_THREADS    = 10;
  static constexpr size_t NUM_ITERATIONS = 1000;
  static constexpr size_t NUM_REMOVE_ALL = NUM_ITERATIONS / 2;

  Info info[NUM_THREADS];
  std::thread threads[NUM_THREADS + 1];
  std::vector< StringItem > items[NUM_THREADS];

  absl::Barrier barrier(NUM_THREADS + 1);

  StringQueue queue;

  for(size_t i = 0; i < NUM_THREADS; ++i)
  {
    info[i].first  = i;
    info[i].second = &items[i];
    threads[i]     = std::thread(
        [](Info* nfo, absl::Barrier* b, StringQueue* q) {
          const int THREAD_ID             = nfo->first;
          std::vector< StringItem >* vPtr = nfo->second;

          // We stagger the removeAll steps among the threads.
          const unsigned int STEP_REMOVE_ALL =
              THREAD_ID * NUM_REMOVE_ALL / NUM_THREADS;

          std::ostringstream oss;
          oss << THREAD_ID;
          Data V(oss.str());

          b->Block();

          size_t newSize;
          absl::Time newMinTime;
          StringItem item;
          for(size_t j = 0; j < NUM_ITERATIONS; ++j)
          {
            const absl::Time TIME =
                absl::Time() + absl::Seconds((j * (j + 3)) % NUM_ITERATIONS);
            int h = q->add(TIME, V);
            q->update(h, TIME);
            if(q->popFront(&item, &newSize, &newMinTime))
            {
              vPtr->push_back(item);
            }
            h = q->add(newMinTime, V);
            q->popLess(newMinTime, vPtr);
            if(q->remove(h, &item, &newSize, &newMinTime))
            {
              vPtr->push_back(item);
            }
            if(j % NUM_REMOVE_ALL == STEP_REMOVE_ALL)
            {
              q->removeAll(vPtr);
            }
          }
        },
        &info[i], &barrier, &queue);
  }

  threads[NUM_THREADS] = std::thread(
      [](absl::Barrier* b, StringQueue* q) {
        b->Block();
        for(size_t i = 0; i < NUM_ITERATIONS; ++i)
        {
          size_t size = q->size();
          ASSERT_GE(size, 0);
          ASSERT_LE(size, NUM_THREADS);
        }
      },
      &barrier, &queue);

  size_t size = 0;
  for(size_t i = 0; i < NUM_THREADS; ++i)
  {
    threads[i].join();
    size += static_cast< int >(items[i].size());
  }
  threads[NUM_THREADS].join();

  ASSERT_EQ(0, queue.size());
  ASSERT_EQ(1000 * NUM_THREADS * 2, size);
}
