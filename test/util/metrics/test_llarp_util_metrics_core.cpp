#include <util/metrics/core.hpp>

#include <array>
#include <thread>

#include <test_util.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;
using namespace metrics;
using namespace ::testing;

MATCHER(IsValid, "")
{
  return arg.valid();
}

static const Category STAT_CAT("A", true);
static const Description desc_A(&STAT_CAT, "A");
static const Description *DESC_A = &desc_A;
static const Description desc_B(&STAT_CAT, "B");
static const Description *DESC_B = &desc_B;

static const Id METRIC_A(DESC_A);
static const Id METRIC_B(DESC_B);

template < typename T >
class CollectorTest : public ::testing::Test
{
};

TYPED_TEST_SUITE_P(CollectorTest);

TYPED_TEST_P(CollectorTest, Collector)
{
  TypeParam collector1(METRIC_A);
  TypeParam collector2(METRIC_B);

  ASSERT_EQ(METRIC_A, collector1.id().description());
  ASSERT_EQ(METRIC_B, collector2.id().description());

  auto record1 = collector1.load();
  ASSERT_EQ(METRIC_A, record1.id.description());
  ASSERT_THAT(record1.data, IsEmpty());

  auto record2 = collector2.load();
  ASSERT_EQ(METRIC_B, record2.id.description());
  ASSERT_THAT(record2.data, IsEmpty());

  const Tags tags;

  collector1.tick(1);
  record1 = collector1.load();
  ASSERT_EQ(METRIC_A, record1.id.description());
  ASSERT_THAT(record1.data, Contains(Key(tags)));
  ASSERT_EQ(1, record1.data.at(tags).count());
  ASSERT_EQ(1, record1.data.at(tags).total());
  ASSERT_EQ(1, record1.data.at(tags).min());
  ASSERT_EQ(1, record1.data.at(tags).max());

  collector1.tick(2);
  record1 = collector1.load();
  ASSERT_EQ(METRIC_A, record1.id.description());
  ASSERT_THAT(record1.data, Contains(Key(tags)));
  ASSERT_EQ(2, record1.data.at(tags).count());
  ASSERT_EQ(3, record1.data.at(tags).total());
  ASSERT_EQ(1, record1.data.at(tags).min());
  ASSERT_EQ(2, record1.data.at(tags).max());

  collector1.tick(-5);
  record1 = collector1.load();
  ASSERT_EQ(METRIC_A, record1.id.description());
  ASSERT_THAT(record1.data, Contains(Key(tags)));
  ASSERT_EQ(3, record1.data.at(tags).count());
  ASSERT_EQ(-2, record1.data.at(tags).total());
  ASSERT_EQ(-5, record1.data.at(tags).min());
  ASSERT_EQ(2, record1.data.at(tags).max());

  collector1.clear();
  record1 = collector1.load();
  ASSERT_EQ(METRIC_A, record1.id.description());
  ASSERT_THAT(record1.data, IsEmpty());

  collector1.tick(3);
  record1 = collector1.loadAndClear();
  ASSERT_EQ(METRIC_A, record1.id.description());
  ASSERT_THAT(record1.data, Contains(Key(tags)));
  ASSERT_EQ(1, record1.data.at(tags).count());
  ASSERT_EQ(3, record1.data.at(tags).total());
  ASSERT_EQ(3, record1.data.at(tags).min());
  ASSERT_EQ(3, record1.data.at(tags).max());

  record1 = collector1.load();
  ASSERT_EQ(METRIC_A, record1.id.description());
  ASSERT_THAT(record1.data, IsEmpty());
}

REGISTER_TYPED_TEST_SUITE_P(CollectorTest, Collector);

using CollectorTestTypes = ::testing::Types< DoubleCollector, IntCollector >;

INSTANTIATE_TYPED_TEST_SUITE_P(MetricsCore, CollectorTest, CollectorTestTypes);

TEST(MetricsCore, Registry)
{
  Registry registry;

  Id idA       = registry.add("MyCategory", "MetricA");
  Id invalidId = registry.add("MyCategory", "MetricA");
  ASSERT_THAT(invalidId, Not(IsValid()));

  Id idA_copy1 = registry.get("MyCategory", "MetricA");
  ASSERT_THAT(idA_copy1, IsValid());
  ASSERT_EQ(idA_copy1, idA);

  Id idA_copy2 = registry.findId("MyCategory", "MetricA");
  ASSERT_THAT(idA_copy2, IsValid());
  ASSERT_EQ(idA_copy2, idA);

  Id idB = registry.get("MyCategory", "MetricB");
  ASSERT_THAT(idB, IsValid());
  ASSERT_EQ(idB, registry.get("MyCategory", "MetricB"));
  ASSERT_EQ(idB, registry.findId("MyCategory", "MetricB"));
  ASSERT_THAT(registry.add("MyCategory", "MetricB"), Not(IsValid()));

  const Category *myCategory = registry.get("MyCategory");
  ASSERT_EQ(myCategory, idA.category());
  ASSERT_EQ(myCategory, idB.category());
  ASSERT_TRUE(myCategory->enabled());

  registry.enable(myCategory, false);
  ASSERT_FALSE(myCategory->enabled());
}

TEST(MetricsCore, RegistryAddr)
{
  Registry registry;
  const Category *CAT_A = registry.add("A");
  const Category *CAT_B = registry.get("B");
  Id METRIC_AA          = registry.add("A", "A");
  Id METRIC_AB          = registry.add("A", "B");
  Id METRIC_AC          = registry.add("A", "C");
  Id METRIC_BA          = registry.get("B", "A");
  Id METRIC_BB          = registry.get("B", "B");
  Id METRIC_BD          = registry.get("B", "D");
  const Category *CAT_C = registry.add("C");
  const Category *CAT_D = registry.add("D");
  Id METRIC_EE          = registry.add("E", "E");
  Id METRIC_FF          = registry.get("F", "F");

  ASSERT_EQ(CAT_A->name(), METRIC_AA.metricName());
  ASSERT_EQ(CAT_B->name(), METRIC_AB.metricName());
  ASSERT_EQ(CAT_A->name(), METRIC_BA.metricName());
  ASSERT_EQ(CAT_B->name(), METRIC_BB.metricName());
  ASSERT_EQ(CAT_C->name(), METRIC_AC.metricName());
  ASSERT_EQ(CAT_D->name(), METRIC_BD.metricName());
  ASSERT_EQ(METRIC_EE.metricName(), METRIC_EE.categoryName());
  ASSERT_EQ(METRIC_FF.metricName(), METRIC_FF.categoryName());
}

TEST(MetricsCore, RegistryOps)
{
  struct
  {
    const char *d_category;
    const char *d_name;
  } METRICS[] = {
      {
          "",
          "",
      },
      {"C0", "M0"},
      {"C0", "M1"},
      {"C1", "M2"},
      {"C3", "M3"},
  };
  const size_t NUM_METRICS = sizeof METRICS / sizeof *METRICS;
  {
    std::set< std::string > categoryNames;

    Registry registry;
    for(size_t i = 0; i < NUM_METRICS; ++i)
    {
      const char *CATEGORY = METRICS[i].d_category;
      const char *NAME     = METRICS[i].d_name;
      categoryNames.insert(CATEGORY);

      // Add a new id and verify the returned properties.
      Id id = registry.add(CATEGORY, NAME);
      ASSERT_TRUE(id.valid()) << id;
      ASSERT_NE(nullptr, id.description());
      ASSERT_NE(nullptr, id.category());
      ASSERT_EQ(id.metricName(), NAME);
      ASSERT_EQ(id.categoryName(), CATEGORY);
      ASSERT_TRUE(id.category()->enabled());

      // Attempt to find the id.
      Id foundId = registry.findId(CATEGORY, NAME);
      ASSERT_TRUE(foundId.valid());
      ASSERT_EQ(foundId, id);

      // Attempt to add the id a second time
      Id invalidId = registry.add(CATEGORY, NAME);
      ASSERT_FALSE(invalidId.valid());

      // Attempt to find the category.
      const Category *foundCat = registry.findCategory(CATEGORY);
      ASSERT_EQ(id.category(), foundCat);
      ASSERT_EQ(nullptr, registry.add(CATEGORY));

      ASSERT_EQ(i + 1, registry.metricCount());
      ASSERT_EQ(categoryNames.size(), registry.categoryCount());
    }
    ASSERT_EQ(NUM_METRICS, registry.metricCount());
    ASSERT_EQ(categoryNames.size(), registry.categoryCount());

    const Category *NEW_CAT = registry.add("NewCategory");
    ASSERT_NE(nullptr, NEW_CAT);
    ASSERT_EQ("NewCategory", NEW_CAT->name());
    ASSERT_TRUE(NEW_CAT->enabled());
  }

  const char *CATEGORIES[]    = {"", "A", "B", "CAT_A", "CAT_B", "name"};
  const size_t NUM_CATEGORIES = sizeof CATEGORIES / sizeof *CATEGORIES;
  {
    Registry registry;
    for(size_t i = 0; i < NUM_CATEGORIES; ++i)
    {
      const char *CATEGORY = CATEGORIES[i];

      const Category *cat = registry.add(CATEGORY);
      ASSERT_NE(nullptr, cat);
      ASSERT_EQ(cat->name(), CATEGORY);
      ASSERT_TRUE(cat->enabled());

      ASSERT_EQ(nullptr, registry.add(CATEGORY));
      ASSERT_EQ(cat, registry.findCategory(CATEGORY));

      Id id = registry.add(CATEGORY, "Metric");
      ASSERT_TRUE(id.valid());
      ASSERT_EQ(cat, id.category());
      ASSERT_EQ(id.categoryName(), CATEGORY);
      ASSERT_EQ(id.metricName(), "Metric");

      ASSERT_EQ(i + 1, registry.metricCount());
      ASSERT_EQ(i + 1, registry.categoryCount());
    }
  }
}

MATCHER_P6(RecordEq, category, name, count, total, min, max, "")
{
  // clang-format off
  return (
    arg.id.categoryName() == std::string(category) &&
    arg.id.metricName() == std::string(name) &&
    arg.data.find(Tags()) != arg.data.end() &&
    arg.data.at(Tags()).count() == count &&
    arg.data.at(Tags()).total() == total &&
    arg.data.at(Tags()).min() == min &&
    arg.data.at(Tags()).max() == max
  );
  // clang-format on
}

MATCHER_P5(RecordEq, id, count, total, min, max, "")
{
  // clang-format off
  return (
    arg.id == id &&
    arg.data.at(Tags()).count() == count &&
    arg.data.at(Tags()).total() == total &&
    arg.data.at(Tags()).min() == min &&
    arg.data.at(Tags()).max() == max
  );
  // clang-format on
}

MATCHER_P4(RecordEq, count, total, min, max, "")
{
  // clang-format off
  return (
    arg.data.at(Tags()).count() == count &&
    arg.data.at(Tags()).total() == total &&
    arg.data.at(Tags()).min() == min &&
    arg.data.at(Tags()).max() == max
  );
  // clang-format on
}

MATCHER_P5(RecordCatEq, category, count, total, min, max, "")
{
  // clang-format off
  return (
    arg.id.categoryName() == std::string(category) &&
    arg.data.at(Tags()).count() == count &&
    arg.data.at(Tags()).total() == total &&
    arg.data.at(Tags()).min() == min &&
    arg.data.at(Tags()).max() == max
  );
  // clang-format on
}

TEST(MetricsCore, RepoBasic)
{
  Registry registry;
  CollectorRepo< double > repo(&registry);

  DoubleCollector *collector1 = repo.defaultCollector("Test", "C1");
  DoubleCollector *collector2 = repo.defaultCollector("Test", "C2");

  ASSERT_NE(collector1, collector2);
  ASSERT_EQ(collector1, repo.defaultCollector("Test", "C1"));

  collector1->tick(1.0);
  collector1->tick(2.0);
  collector2->tick(4.0);

  std::vector< TaggedRecords< double > > records =
      repo.collectAndClear(registry.get("Test"));
  EXPECT_THAT(records, SizeIs(2));
  // clang-format off
  EXPECT_THAT(
    records,
    ElementsAre(
      RecordEq("Test", "C1", 2u, 3, 1, 2),
      RecordEq("Test", "C2", 1u, 4, 4, 4)
    )
  );
  // clang-format on

  for(const auto &rec : records)
  {
    std::cout << rec << std::endl;
  }
}

TEST(MetricsCore, RepoCollect)
{
  Registry registry;
  std::array< const char *, 3 > CATEGORIES = {"A", "B", "C"};
  std::array< const char *, 3 > METRICS    = {"A", "B", "C"};
  const int NUM_COLS                       = 3;

  for(int i = 0; i < static_cast< int >(CATEGORIES.size()); ++i)
  {
    CollectorRepo< int > repo(&registry);

    for(int j = 0; j < static_cast< int >(CATEGORIES.size()); ++j)
    {
      const char *CATEGORY = CATEGORIES[j];
      for(int k = 0; k < static_cast< int >(METRICS.size()); ++k)
      {
        Id metric = registry.get(CATEGORY, METRICS[k]);
        for(int l = 0; l < NUM_COLS; ++l)
        {
          IntCollector *iCol = repo.addCollector(metric).get();
          if(i == j)
          {
            iCol->set(k, 2 * k, -k, k);
          }
          else
          {
            iCol->set(100, 100, 100, 100);
          }
        }
      }
    }

    // Collect records for the metrics we're testing
    {
      const char *CATEGORY     = CATEGORIES[i];
      const Category *category = registry.get(CATEGORY);

      std::vector< TaggedRecords< int > > records = repo.collect(category);

      ASSERT_THAT(records, SizeIs(METRICS.size()));
      // clang-format off
      ASSERT_THAT(
        records,
        UnorderedElementsAre(
          RecordEq(CATEGORY, "A", 0u, 0, 0, 0),
          RecordEq(CATEGORY, "B", 3u, 6, -1, 1),
          RecordEq(CATEGORY, "C", 6u, 12, -2, 2)
        )
      );
      // clang-format on

      // Validate initial values.
      for(int j = 0; j < static_cast< int >(METRICS.size()); ++j)
      {
        Id metric = registry.get(CATEGORY, METRICS[j]);

        auto collectors = repo.allCollectors(metric);
        for(int k = 0; k < static_cast< int >(collectors.size()); ++k)
        {
          TaggedRecords< int > EI(metric);
          EI.data[Tags()]             = Record< int >(j, 2 * j, -j, j);
          TaggedRecords< int > record = collectors[k]->load();
          ASSERT_EQ(record, EI);
        }
      }
    }

    // Verify the collectors for other categories haven't changed.
    for(int j = 0; j < static_cast< int >(CATEGORIES.size()); ++j)
    {
      if(i == j)
      {
        continue;
      }
      const char *CATEGORY = CATEGORIES[j];

      for(int k = 0; k < static_cast< int >(METRICS.size()); ++k)
      {
        Id metric       = registry.get(CATEGORY, METRICS[j]);
        auto collectors = repo.allCollectors(metric);

        for(int l = 0; l < static_cast< int >(collectors.size()); ++l)
        {
          TaggedRecords< int > record = collectors[k]->load();
          ASSERT_THAT(record, RecordEq(metric, 100u, 100, 100, 100));
        }
      }
    }
  }
}

MATCHER_P2(WithinWindow, expectedTime, window, "")
{
  auto begin = expectedTime - window;
  auto end   = expectedTime + window;

  return (begin < arg && arg < end);
}

const Category *
firstCategory(
    const absl::variant< SampleGroup< double >, SampleGroup< int > > &g)
{
  return absl::visit(
      [](const auto &group) -> const Category * {
        EXPECT_THAT(group, Not(IsEmpty()));
        const Category *value = group.begin()->id.category();
        for(const auto &record : group.records())
        {
          EXPECT_EQ(value, record.id.category());
        }
        return value;
      },
      g);
}

TEST(MetricsCore, ManagerCollectSample1)
{
  const char *CATEGORIES[] = {"A", "B", "C", "Test", "12312category"};
  const int NUM_CATEGORIES = sizeof(CATEGORIES) / sizeof(*CATEGORIES);

  const char *METRICS[] = {"A", "B", "C", "MyMetric", "90123metric"};
  const int NUM_METRICS = sizeof(METRICS) / sizeof(*METRICS);

  Manager manager;
  CollectorRepo< double > &rep = manager.doubleCollectorRepo();

  for(int i = 0; i < NUM_CATEGORIES; ++i)
  {
    for(int j = 0; j < NUM_METRICS; ++j)
    {
      rep.defaultCollector(CATEGORIES[i], METRICS[j])->tick(1);
    }
  }

  absl::Time start = absl::Now();
  std::this_thread::sleep_for(std::chrono::microseconds(100000));

  Records records;
  Sample sample = manager.collectSample(records, false);

  absl::Duration window = absl::Now() - start;
  absl::Time now        = absl::Now();
  ASSERT_EQ(NUM_CATEGORIES * NUM_METRICS, records.doubleRecords.size());
  ASSERT_EQ(NUM_CATEGORIES * NUM_METRICS, sample.recordCount());
  ASSERT_EQ(NUM_CATEGORIES, sample.groupCount());
  ASSERT_THAT(sample.sampleTime(), WithinWindow(now, absl::Milliseconds(10)));

  for(size_t i = 0; i < sample.groupCount(); ++i)
  {
    const SampleGroup< double > &group =
        absl::get< SampleGroup< double > >(sample.group(i));
    ASSERT_EQ(NUM_METRICS, group.size());
    ASSERT_THAT(group.samplePeriod(),
                WithinWindow(window, absl::Milliseconds(10)))
        << group;

    string_view name = group.records()[0].id.categoryName();
    for(const auto &record : group.records())
    {
      ASSERT_THAT(record, RecordCatEq(name, 1u, 1, 1, 1));
    }
  }
  for(size_t i = 0; i < NUM_CATEGORIES; ++i)
  {
    for(size_t j = 0; j < NUM_METRICS; ++j)
    {
      DoubleCollector *col = rep.defaultCollector(CATEGORIES[i], METRICS[j]);
      auto record          = col->load();
      ASSERT_THAT(record, RecordEq(1u, 1, 1, 1));
    }
  }

  records.doubleRecords.clear();
  records.intRecords.clear();
  sample = manager.collectSample(records, true);

  ASSERT_EQ(NUM_CATEGORIES * NUM_METRICS, records.doubleRecords.size());
  ASSERT_EQ(NUM_CATEGORIES * NUM_METRICS, sample.recordCount());
  ASSERT_EQ(NUM_CATEGORIES, sample.groupCount());

  for(size_t i = 0; i < NUM_CATEGORIES; ++i)
  {
    for(size_t j = 0; j < NUM_METRICS; ++j)
    {
      DoubleCollector *col = rep.defaultCollector(CATEGORIES[i], METRICS[j]);
      auto record          = col->load();
      ASSERT_EQ(TaggedRecords< double >(record.id), record);
    }
  }
}

TEST(MetricsCore, ManagerCollectSample2)
{
  const char *CATEGORIES[] = {"A", "B", "C", "Test", "12312category"};
  const int NUM_CATEGORIES = sizeof(CATEGORIES) / sizeof(*CATEGORIES);

  const char *METRICS[] = {"A", "B", "C", "MyMetric", "90123metric"};
  const int NUM_METRICS = sizeof(METRICS) / sizeof(*METRICS);

  Manager manager;
  std::vector< const Category * > allCategories;

  CollectorRepo< double > &rep = manager.doubleCollectorRepo();
  Registry &reg                = manager.registry();
  for(size_t i = 0; i < NUM_CATEGORIES; ++i)
  {
    const Category *cat = reg.get(CATEGORIES[i]);
    ASSERT_NE(nullptr, cat);
    allCategories.push_back(cat);
  }

  test::CombinationIterator< const Category * > combIt{allCategories};
  do
  {
    for(size_t i = 0; i < NUM_CATEGORIES; ++i)
    {
      for(size_t j = 0; j < NUM_METRICS; ++j)
      {
        DoubleCollector *col = rep.defaultCollector(CATEGORIES[i], METRICS[j]);
        col->clear();
        col->tick(1);
      }
    }

    // Test without a reset.
    std::vector< const Category * > cats = combIt.currentCombo;
    Records records;
    Sample sample = manager.collectSample(
        records, absl::Span< const Category * >{cats}, false);

    ASSERT_EQ(NUM_METRICS * cats.size(), sample.recordCount());
    ASSERT_EQ(cats.size(), sample.groupCount());
    for(size_t i = 0; i < NUM_CATEGORIES; ++i)
    {
      // Verify the correct categories are in the sample (once)
      const Category *CATEGORY = allCategories[i];
      bool found               = false;
      for(size_t j = 0; j < sample.groupCount(); ++j)
      {
        if(CATEGORY == firstCategory(sample.group(j)))
        {
          found = true;
        }
      }
      ASSERT_EQ(found, combIt.includesElement(i));
    }
    for(size_t i = 0; i < NUM_CATEGORIES; ++i)
    {
      for(size_t j = 0; j < NUM_METRICS; ++j)
      {
        DoubleCollector *col = rep.defaultCollector(CATEGORIES[i], METRICS[j]);
        TaggedRecords< double > record = col->load();
        ASSERT_THAT(record, RecordEq(1u, 1, 1, 1));
      }
    }
    Records records2;

    // Test with a reset.
    sample = manager.collectSample(records2,
                                   absl::Span< const Category * >{cats}, true);

    ASSERT_EQ(NUM_METRICS * cats.size(), sample.recordCount());
    ASSERT_EQ(cats.size(), sample.groupCount());
    ASSERT_EQ(records, records2);
    for(size_t i = 0; i < NUM_CATEGORIES; ++i)
    {
      // Verify the correct categories are in the sample
      const Category *CATEGORY = allCategories[i];
      bool found               = false;
      for(size_t j = 0; j < sample.groupCount(); ++j)
      {
        if(CATEGORY == firstCategory(sample.group(j)))
        {
          found = true;
        }
      }
      ASSERT_EQ(found, combIt.includesElement(i));
    }
    for(size_t i = 0; i < NUM_CATEGORIES; ++i)
    {
      for(size_t j = 0; j < NUM_METRICS; ++j)
      {
        DoubleCollector *col = rep.defaultCollector(CATEGORIES[i], METRICS[j]);
        TaggedRecords< double > record = col->load();
        if(combIt.includesElement(i))
        {
          ASSERT_EQ(TaggedRecords< double >(record.id), record);
        }
        else
        {
          ASSERT_THAT(record, RecordEq(1u, 1, 1, 1));
        }
      }
    }

  } while(combIt.next());
}

struct MockPublisher : public Publisher
{
  std::atomic_int invocations;
  std::vector< TaggedRecords< double > > recordBuffer;
  std::vector< TaggedRecords< double > > sortedRecords;
  Sample m_sample;

  std::set< absl::Duration > times;

  void
  publish(const Sample &sample) override
  {
    invocations++;

    m_sample.clear();
    recordBuffer.clear();
    sortedRecords.clear();
    times.clear();

    m_sample.sampleTime(sample.sampleTime());

    if(sample.recordCount() == 0)
    {
      return;
    }

    recordBuffer.reserve(sample.recordCount());

    for(const auto &_s : sample)
    {
      ASSERT_TRUE(absl::holds_alternative< SampleGroup< double > >(_s));
      const auto &s = absl::get< SampleGroup< double > >(_s);
      auto git      = s.begin();
      ASSERT_NE(git, s.end());
      recordBuffer.push_back(*git);
      TaggedRecords< double > *head = &recordBuffer.back();
      for(++git; git != s.end(); ++git)
      {
        recordBuffer.push_back(*git);
      }
      m_sample.pushGroup(head, s.size(), samplePeriod(s));
      times.insert(s.samplePeriod());
    }

    sortedRecords = recordBuffer;
    std::sort(sortedRecords.begin(), sortedRecords.end(),
              [](const auto &lhs, const auto &rhs) { return lhs.id < rhs.id; });
  }

  void
  reset()
  {
    invocations = 0;
    m_sample.clear();
    recordBuffer.clear();
    sortedRecords.clear();
    times.clear();
  }

  int
  indexOf(const Id &id)
  {
    TaggedRecords< double > searchRecord(id);
    auto it = std::lower_bound(
        sortedRecords.begin(), sortedRecords.end(), searchRecord,
        [](const auto &lhs, const auto &rhs) { return lhs.id < rhs.id; });

    if(it == sortedRecords.end())
    {
      return -1;
    }
    return (it->id == id) ? it - sortedRecords.begin() : -1;
  }

  bool
  contains(const Id &id)
  {
    return indexOf(id) != -1;
  }
};

TEST(MetricsCore, ManagerAddCatPub)
{
  const char *CATEGORIES[] = {"A", "B", "C", "Test", "12312category"};
  const int NUM_CATEGORIES = sizeof(CATEGORIES) / sizeof(*CATEGORIES);
  const int NUM_PUBLISHERS = 4;

  std::multimap< const char *, std::shared_ptr< Publisher > > publishers;

  Manager manager;
  Registry &registry = manager.registry();
  for(int i = 0; i < NUM_CATEGORIES; ++i)
  {
    for(int j = 0; j < NUM_PUBLISHERS; ++j)
    {
      auto globalPub = std::make_shared< MockPublisher >();
      manager.addPublisher(CATEGORIES[i], globalPub);
      publishers.emplace(CATEGORIES[i], globalPub);
    }
  }

  for(int i = 0; i < NUM_CATEGORIES; ++i)
  {
    const char *CATEGORY               = CATEGORIES[i];
    const Category *CAT                = registry.get(CATEGORY);
    std::vector< Publisher * > results = manager.publishersForCategory(CAT);
    ASSERT_EQ(NUM_PUBLISHERS, results.size());

    auto it = publishers.lower_bound(CATEGORY);

    for(const auto &pub : results)
    {
      ASSERT_EQ(pub, it->second.get());
      ++it;
    }
  }
}

TEST(MetricsCore, ManagerEnableAll)
{
  const char *CATEGORIES[] = {"A", "B", "C", "Test", "12312category"};
  const int NUM_CATEGORIES = sizeof(CATEGORIES) / sizeof(*CATEGORIES);

  Manager manager;
  Registry &registry = manager.registry();

  for(int i = 0; i < NUM_CATEGORIES; ++i)
  {
    const Category *CAT = registry.get(CATEGORIES[i]);
    ASSERT_TRUE(CAT->enabled());
    manager.enableCategory(CAT, false);
    ASSERT_FALSE(CAT->enabled());
    manager.enableCategory(CAT, true);
    ASSERT_TRUE(CAT->enabled());

    manager.enableCategory(CATEGORIES[i], false);
    ASSERT_FALSE(CAT->enabled());
    manager.enableCategory(CATEGORIES[i], true);
    ASSERT_TRUE(CAT->enabled());
  }

  manager.enableAll(false);
  for(int i = 0; i < NUM_CATEGORIES; ++i)
  {
    ASSERT_FALSE(registry.get(CATEGORIES[i])->enabled());
  }

  manager.enableAll(true);
  for(int i = 0; i < NUM_CATEGORIES; ++i)
  {
    ASSERT_TRUE(registry.get(CATEGORIES[i])->enabled());
  }
}

TEST(MetricsCore, PublishAll)
{
  const char *CATEGORIES[] = {"A", "B", "C", "Test", "12312category"};
  const int NUM_CATEGORIES = sizeof(CATEGORIES) / sizeof(*CATEGORIES);

  const char *METRICS[] = {"A", "B", "C", "MyMetric", "903metric"};
  const int NUM_METRICS = sizeof(METRICS) / sizeof(*METRICS);

  Manager manager;
  Registry &registry                  = manager.registry();
  CollectorRepo< double > &repository = manager.doubleCollectorRepo();

  auto globalPub = std::make_shared< MockPublisher >();

  manager.addGlobalPublisher(globalPub);

  std::vector< const Category * > allCategories;
  for(int i = 0; i < NUM_CATEGORIES; ++i)
  {
    const Category *CAT = registry.get(CATEGORIES[i]);
    auto mockPubCat     = std::make_shared< MockPublisher >();
    manager.addPublisher(CAT, mockPubCat);
    allCategories.push_back(CAT);
  }

  test::CombinationIterator< const Category * > combIt(allCategories);
  do
  {
    for(int i = 0; i < NUM_CATEGORIES; ++i)
    {
      for(int j = 0; j < NUM_METRICS; ++j)
      {
        DoubleCollector *col =
            repository.defaultCollector(CATEGORIES[i], METRICS[j]);
        col->clear();
        col->tick(1);
      }
    }

    std::set< const Category * > excludedSet;
    for(int i = 0; i < NUM_CATEGORIES; ++i)
    {
      if(!combIt.includesElement(i))
      {
        excludedSet.insert(allCategories[i]);
      }
    }
    ASSERT_EQ(allCategories.size(),
              excludedSet.size() + combIt.currentCombo.size());

    // Publish the records.
    absl::Time tmStamp = absl::Now();
    manager.publishAllExcluding(excludedSet);

    if(combIt.currentCombo.empty())
    {
      ASSERT_EQ(0, globalPub->invocations.load());
    }
    else
    {
      ASSERT_EQ(1, globalPub->invocations.load());
      ASSERT_THAT(globalPub->m_sample.sampleTime(),
                  WithinWindow(tmStamp, absl::Milliseconds(10)));
      ASSERT_EQ(combIt.currentCombo.size(), globalPub->m_sample.groupCount());
    }

    // Verify the correct "specific" publishers have been invoked.
    for(int i = 0; i < NUM_CATEGORIES; ++i)
    {
      for(int j = 0; j < NUM_METRICS; ++j)
      {
        Id id = registry.get(CATEGORIES[i], METRICS[j]);
        ASSERT_EQ(combIt.includesElement(i), globalPub->contains(id));
      }

      const int EXP_INV = combIt.includesElement(i) ? 1 : 0;
      std::vector< Publisher * > pubs =
          manager.publishersForCategory(allCategories[i]);
      MockPublisher *specPub = (MockPublisher *)pubs.front();
      ASSERT_EQ(EXP_INV, specPub->invocations.load());
      specPub->reset();
    }
    globalPub->reset();
  } while(combIt.next());
}
