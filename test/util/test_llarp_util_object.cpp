#include <util/object.hpp>

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
  int handle1         = -1;
  int handle2         = -1;

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

    absl::optional< double > result = catalog.find(handle1);
    ASSERT_TRUE(result);
    ASSERT_EQ(value2, result);
    catalog.remove(handle2);
  }
}

TEST(Catalog, Iterator)
{
  static constexpr size_t THREAD_COUNT    = 10;
  static constexpr size_t ITERATION_COUNT = 1000;
  std::array< std::thread, THREAD_COUNT + 3 > threads;

  using llarp::util::Barrier;
  using Iterator = CatalogIterator< int >;
  using Cat      = Catalog< int >;

  Barrier barrier(THREAD_COUNT + 3);
  Cat catalog;

  // Repeatedly remove + add values from the catalog
  for(size_t i = 0; i < THREAD_COUNT; ++i)
  {
    threads[i] = std::thread(
        [](Barrier *barrier, Cat *catalog, int id) {
          barrier->Block();
          for(size_t i = 0; i < ITERATION_COUNT; ++i)
          {
            int h                     = catalog->add(id);
            absl::optional< int > res = catalog->find(h);
            ASSERT_TRUE(res);
            ASSERT_EQ(res.value(), id);
            ASSERT_TRUE(catalog->replace(-id - 1, h));
            res = catalog->find(h);
            ASSERT_TRUE(res);
            ASSERT_EQ(-id - 1, res.value());
            int removed = -1;
            ASSERT_TRUE(catalog->remove(h, &removed));
            ASSERT_EQ(removed, -id - 1);
            ASSERT_FALSE(catalog->find(h));
          }
        },
        &barrier, &catalog, i);
  }

  // Verify the length constraint is never violated
  threads[THREAD_COUNT] = std::thread(
      [](Barrier *barrier, Cat *catalog) {
        barrier->Block();
        for(size_t i = 0; i < ITERATION_COUNT; ++i)
        {
          size_t size = catalog->size();
          ASSERT_LE(size, THREAD_COUNT);
        }
      },
      &barrier, &catalog);

  // Verify that iteration always produces a valid state
  threads[THREAD_COUNT + 1] = std::thread(
      [](Barrier *barrier, Cat *catalog) {
        barrier->Block();
        for(size_t i = 0; i < ITERATION_COUNT; ++i)
        {
          int arr[100];
          size_t size = 0;
          for(Iterator it(*catalog); it; ++it)
          {
            arr[size++] = it().second;
          }
          for(int i = 0; i < 100; i++)
          {
            // value must be valid
            bool present = false;
            for(int id = 0; id < static_cast< int >(THREAD_COUNT); id++)
            {
              if(id == arr[i] || -id - 1 == arr[i])
              {
                present = true;
                break;
              }
            }
            ASSERT_TRUE(present);

            // no duplicate should be there
            for(size_t j = i + 1; j < size; j++)
            {
              ASSERT_NE(arr[i], arr[j]);
            }
          }
        }
      },
      &barrier, &catalog);

  // And that we don't have an invalid catalog
  threads[THREAD_COUNT + 2] = std::thread(
      [](Barrier *barrier, Cat *catalog) {
        barrier->Block();
        for(size_t i = 0; i < ITERATION_COUNT; ++i)
        {
          catalog->verify();
        }
      },
      &barrier, &catalog);

  for(std::thread &t : threads)
  {
    t.join();
  }
}
