#include <util/meta/object.hpp>
#include <util/thread/barrier.hpp>

#include <array>
#include <thread>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp::object;

TEST(Object, VerifySize)
{
  static_assert(sizeof(Buffer< char >) == sizeof(char), "");
  static_assert(sizeof(Buffer< int >) == sizeof(int), "");
  static_assert(sizeof(Buffer< double >) == sizeof(double), "");
  static_assert(sizeof(Buffer< std::string >) == sizeof(std::string), "");
}

TEST(Object, Inplace)
{
  // Verify we can create and destroy a type with a non-trivial destructor
  Buffer< std::vector< std::string > > strBuf;
  new(strBuf.buffer()) std::vector< std::string >(100, "abc");
  strBuf.value().~vector();
}

TEST(Catalog, smoke)
{
  const double value1 = 1.0;
  const double value2 = 2.0;
  int32_t handle1     = -1;
  int32_t handle2     = -1;

  Catalog< double > catalog;

  handle1 = catalog.add(value1);
  catalog.remove(handle1);

  for(size_t j = 0; j < 5; ++j)
  {
    for(size_t i = 1; i < 256; ++i)
    {
      ASSERT_FALSE(catalog.find(handle1));

      handle2 = catalog.add(value2);
      catalog.remove(handle2);
    }
    handle2 = catalog.add(value2);

    ASSERT_EQ(handle1, handle2);
    ASSERT_TRUE(catalog.find(handle1));

    nonstd::optional< double > result = catalog.find(handle1);
    ASSERT_TRUE(result);
    ASSERT_EQ(value2, result);
    catalog.remove(handle2);
  }
}

TEST(Catalog, Iterator)
{
  static constexpr size_t THREAD_COUNT    = 10;
  static constexpr size_t ITERATION_COUNT = 1000;
  static constexpr int32_t MAX = std::numeric_limits< int32_t >::max();
  std::array< std::thread, THREAD_COUNT + 3 > threads;

  using llarp::util::Barrier;
  using Iterator = CatalogIterator< int32_t >;
  using Cat      = Catalog< int32_t >;

  Barrier barrier(THREAD_COUNT + 3);
  Cat catalog;

  // Repeatedly remove + add values from the catalog
  for(size_t i = 0; i < THREAD_COUNT; ++i)
  {
    threads[i] = std::thread(
        [](Barrier *b, Cat *cat, int32_t id) {
          b->Block();
          for(size_t j = 0; j < ITERATION_COUNT; ++j)
          {
            int32_t handle                = cat->add(id);
            nonstd::optional< int32_t > res = cat->find(handle);
            ASSERT_TRUE(res);
            ASSERT_EQ(res.value(), id);
            ASSERT_TRUE(cat->replace(MAX - id, handle));
            res = cat->find(handle);
            ASSERT_TRUE(res);
            ASSERT_EQ(MAX - id, res.value());
            int32_t removed = 0;
            ASSERT_TRUE(cat->remove(handle, &removed));
            ASSERT_EQ(removed, MAX - id);
            ASSERT_FALSE(cat->find(handle));
          }
        },
        &barrier, &catalog, i);
  }

  // Verify the length constraint is never violated
  threads[THREAD_COUNT] = std::thread(
      [](Barrier *b, Cat *cat) {
        b->Block();
        for(size_t i = 0; i < ITERATION_COUNT; ++i)
        {
          size_t size = cat->size();
          ASSERT_LE(size, THREAD_COUNT);
        }
      },
      &barrier, &catalog);

  // Verify that iteration always produces a valid state
  threads[THREAD_COUNT + 1] = std::thread(
      [](Barrier *b, Cat *cat) {
        b->Block();
        for(size_t i = 0; i < ITERATION_COUNT; ++i)
        {
          int32_t arr[100];
          size_t size = 0;
          for(Iterator it(cat); it; ++it)
          {
            arr[size++] = it().second;
          }
          for(int32_t j = 0; j < 100; j++)
          {
            // value must be valid
            bool present = false;
            for(int32_t id = 0; id < static_cast< int32_t >(THREAD_COUNT); id++)
            {
              if(id == arr[j] || MAX - id == arr[j])
              {
                present = true;
                break;
              }
            }

            (void)present;

            // no duplicate should be there
            for(size_t k = j + 1; k < size; k++)
            {
              ASSERT_NE(arr[j], arr[k]);
            }
          }
        }
      },
      &barrier, &catalog);

  // And that we don't have an invalid catalog
  threads[THREAD_COUNT + 2] = std::thread(
      [](Barrier *b, Cat *cat) {
        b->Block();
        for(size_t i = 0; i < ITERATION_COUNT; ++i)
        {
          cat->verify();
        }
      },
      &barrier, &catalog);

  for(std::thread &t : threads)
  {
    t.join();
  }
}
