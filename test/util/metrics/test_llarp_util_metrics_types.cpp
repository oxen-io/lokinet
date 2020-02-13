#include <util/metrics/types.hpp>

#include <array>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;
using namespace metrics;
using namespace ::testing;

using RecordT      = metrics::Record< double >;
using TagRecordT   = metrics::TaggedRecords< double >;
using SampleGroupT = metrics::SampleGroup< double >;

struct MetricFormatSpecTestData
{
  float m_scale;
  const char *m_spec;
  double m_value;
  const char *m_expected;
};

struct MetricFormatSpecTest : public TestWithParam< MetricFormatSpecTestData >
{
};

TEST_P(MetricFormatSpecTest, print)
{
  auto d = GetParam();

  metrics::FormatSpec spec(d.m_scale, d.m_spec);
  std::ostringstream stream;
  metrics::FormatSpec::format(stream, d.m_value, spec);

  ASSERT_EQ(d.m_expected, stream.str());
}

MetricFormatSpecTestData metricFormatTestData[] = {
    MetricFormatSpecTestData{0.0, "", 1.5, ""},
    MetricFormatSpecTestData{1.0, "%.4f", 1.5, "1.5000"},
    MetricFormatSpecTestData{1.0, "%.0f", 2.0, "2"},
    MetricFormatSpecTestData{1.0, "%.0f", 1.1, "1"},
    MetricFormatSpecTestData{1.0, "%.0f", 1.5, "2"},
    MetricFormatSpecTestData{1.0, "%.0f", 1.7, "2"},
    MetricFormatSpecTestData{1.0, "%.0f", 3.0, "3"},
    MetricFormatSpecTestData{2.0, "%.0f", 3.0, "6"},
    MetricFormatSpecTestData{2.0, "%.1f", 1.1, "2.2"}};

INSTANTIATE_TEST_SUITE_P(MetricsTypes, MetricFormatSpecTest,
                         ValuesIn(metricFormatTestData));

TEST(MetricsTypes, Format)
{
  metrics::Format format;

  format.setSpec(metrics::Publication::Type::Max,
                 metrics::FormatSpec(1.0, "%0.2f"));
  format.setSpec(metrics::Publication::Type::Total,
                 metrics::FormatSpec(2.0, "%0.3f"));

  ASSERT_EQ(nullptr, format.specFor(metrics::Publication::Type::Avg));
  auto ptr = format.specFor(metrics::Publication::Type::Total);
  ASSERT_NE(nullptr, ptr);
  ASSERT_EQ("%0.3f", ptr->m_format);
  ASSERT_DOUBLE_EQ(2.0, ptr->m_scale);
  ptr = format.specFor(metrics::Publication::Type::Max);
  ASSERT_NE(nullptr, ptr);
  ASSERT_EQ("%0.2f", ptr->m_format);
  ASSERT_DOUBLE_EQ(1.0, ptr->m_scale);

  format.clear();

  ASSERT_EQ(nullptr, format.specFor(metrics::Publication::Type::Total));
  ASSERT_EQ(nullptr, format.specFor(metrics::Publication::Type::Max));
}

TEST(MetricsTypes, CatContainer)
{
  std::array< metrics::CategoryContainer, 10 > containers;
  {
    metrics::Category c("A");
    for(size_t i = 0; i < containers.size(); ++i)
    {
      c.registerContainer(&containers[i]);
      metrics::CategoryContainer *next = (0 == i) ? 0 : &containers[i - 1];
      ASSERT_EQ(&c, containers[i].m_category);
      ASSERT_TRUE(containers[i].m_enabled);
      ASSERT_EQ(next, containers[i].m_nextCategory);
    }

    for(size_t i = 0; i < containers.size(); ++i)
    {
      metrics::CategoryContainer *next = (0 == i) ? 0 : &containers[i - 1];
      ASSERT_EQ(&c, containers[i].m_category);
      ASSERT_TRUE(containers[i].m_enabled);
      ASSERT_EQ(next, containers[i].m_nextCategory);
    }

    const std::atomic_bool *enabled = &c.enabledRaw();

    c.enabled(false);

    ASSERT_FALSE(*enabled);
    ASSERT_EQ(&c.enabledRaw(), enabled);

    for(size_t i = 0; i < containers.size(); ++i)
    {
      metrics::CategoryContainer *next = (0 == i) ? 0 : &containers[i - 1];
      ASSERT_EQ(&c, containers[i].m_category);
      ASSERT_FALSE(containers[i].m_enabled);
      ASSERT_EQ(next, containers[i].m_nextCategory);
    }

    c.enabled(true);

    ASSERT_TRUE(*enabled);
    ASSERT_EQ(&c.enabledRaw(), enabled);

    for(size_t i = 0; i < containers.size(); ++i)
    {
      metrics::CategoryContainer *next = (0 == i) ? 0 : &containers[i - 1];
      ASSERT_EQ(&c, containers[i].m_category);
      ASSERT_TRUE(containers[i].m_enabled);
      ASSERT_EQ(next, containers[i].m_nextCategory);
    }
  }
  for(const auto &container : containers)
  {
    ASSERT_THAT(container.m_category, IsNull());
    ASSERT_FALSE(container.m_enabled);
    ASSERT_THAT(container.m_nextCategory, IsNull());
  }
}

TEST(MetricsTypes, Record)
{
  RecordT r;
  ASSERT_GT(r.min(), r.max());
}

TEST(MetricsTypes, Sample)
{
  metrics::Category myCategory("MyCategory");
  metrics::Description descA(&myCategory, "MetricA");
  metrics::Description descB(&myCategory, "MetricB");
  metrics::Description descC(&myCategory, "MetricC");

  metrics::Id metricA(&descA);
  metrics::Id metricB(&descB);
  metrics::Id metricC(&descC);

  absl::Time timeStamp = absl::Now();
  RecordT recordA(0, 0, 0, 0);
  RecordT recordB(1, 2, 3, 4);
  RecordT recordC(4, 3, 2, 1);

  TagRecordT tagRecordA(metricA, {{{}, recordA}});
  TagRecordT tagRecordB(metricB, {{{}, recordB}});
  TagRecordT tagRecordC(metricC, {{{}, recordC}});

  TagRecordT buffer1[] = {tagRecordA, tagRecordB};
  std::vector< TagRecordT > buffer2;
  buffer2.push_back(tagRecordC);

  metrics::Sample sample;
  sample.sampleTime(timeStamp);
  sample.pushGroup(buffer1, sizeof(buffer1) / sizeof(*buffer1),
                   absl::Seconds(1.0));
  sample.pushGroup(buffer2.data(), buffer2.size(), absl::Seconds(2.0));

  ASSERT_EQ(timeStamp, sample.sampleTime());
  ASSERT_EQ(2u, sample.groupCount());
  ASSERT_EQ(3u, sample.recordCount());
  ASSERT_TRUE(absl::holds_alternative< SampleGroupT >(sample.group(0)));
  ASSERT_TRUE(absl::holds_alternative< SampleGroupT >(sample.group(1)));

  const SampleGroupT s0 = absl::get< SampleGroupT >(sample.group(0));
  const SampleGroupT s1 = absl::get< SampleGroupT >(sample.group(1));
  ASSERT_EQ(absl::Seconds(1), s0.samplePeriod());
  ASSERT_EQ(buffer1, s0.records().data());
  ASSERT_EQ(2, s0.size());

  ASSERT_EQ(absl::Seconds(2), s1.samplePeriod());
  ASSERT_EQ(buffer2.data(), s1.records().data());
  ASSERT_EQ(1, s1.size());

  for(auto sampleIt = sample.begin(); sampleIt != sample.end(); ++sampleIt)
  {
    const auto &s = absl::get< SampleGroupT >(*sampleIt);
    for(auto groupIt = s.begin(); groupIt != s.end(); ++groupIt)
    {
      std::cout << *groupIt << std::endl;
    }
  }
}

struct SampleTest
    : public ::testing::TestWithParam< std::pair< absl::Time, std::string > >
{
  metrics::Category cat_A;
  metrics::Description DESC_A;
  metrics::Description DESC_B;
  metrics::Description DESC_C;
  metrics::Description DESC_D;
  metrics::Description DESC_E;
  metrics::Description DESC_F;
  metrics::Description DESC_G;

  metrics::Id id_A;
  metrics::Id id_B;
  metrics::Id id_C;
  metrics::Id id_D;
  metrics::Id id_E;
  metrics::Id id_F;
  metrics::Id id_G;

  std::vector< TagRecordT > recordBuffer;

  SampleTest()
      : cat_A("A", true)
      , DESC_A(&cat_A, "A")
      , DESC_B(&cat_A, "B")
      , DESC_C(&cat_A, "C")
      , DESC_D(&cat_A, "D")
      , DESC_E(&cat_A, "E")
      , DESC_F(&cat_A, "F")
      , DESC_G(&cat_A, "G")
      , id_A(&DESC_A)
      , id_B(&DESC_B)
      , id_C(&DESC_C)
      , id_D(&DESC_D)
      , id_E(&DESC_E)
      , id_F(&DESC_F)
      , id_G(&DESC_G)
  {
    recordBuffer.emplace_back(
        metrics::Id(0),
        TaggedRecordsData< double >{{metrics::Tags(), RecordT(1, 1, 1, 1)}});
    recordBuffer.emplace_back(
        id_A,
        TaggedRecordsData< double >{{metrics::Tags(), RecordT(2, 2, 2, 2)}});
    recordBuffer.emplace_back(
        id_B,
        TaggedRecordsData< double >{{metrics::Tags(), RecordT(3, 3, 3, 3)}});
    recordBuffer.emplace_back(
        id_C,
        TaggedRecordsData< double >{{metrics::Tags(), RecordT(4, 4, 4, 4)}});
    recordBuffer.emplace_back(
        id_D,
        TaggedRecordsData< double >{{metrics::Tags(), RecordT(5, 5, 5, 5)}});
    recordBuffer.emplace_back(
        id_E,
        TaggedRecordsData< double >{{metrics::Tags(), RecordT(6, 6, 6, 6)}});
    recordBuffer.emplace_back(
        id_F,
        TaggedRecordsData< double >{{metrics::Tags(), RecordT(7, 7, 7, 7)}});
    recordBuffer.emplace_back(
        id_G,
        TaggedRecordsData< double >{{metrics::Tags(), RecordT(8, 8, 8, 8)}});
    recordBuffer.emplace_back(
        id_A,
        TaggedRecordsData< double >{{metrics::Tags(), RecordT(9, 9, 9, 9)}});
  }
};

std::pair< std::vector< metrics::SampleGroup< double > >, size_t >
generate(const std::string &specification,
         const std::vector< TagRecordT > &recordBuffer)
{
  const char *c = specification.c_str();

  std::vector< metrics::SampleGroup< double > > groups;
  size_t size = 0;

  const TagRecordT *head    = recordBuffer.data();
  const TagRecordT *current = head;
  while(*c)
  {
    int numRecords = *(c + 1) - '0';

    int elapsedTime = *(c + 3) - '0';

    if(head + recordBuffer.size() < current + numRecords)
    {
      current = head;
    }
    groups.emplace_back(current, numRecords, absl::Seconds(elapsedTime));

    size += numRecords;
    current += numRecords;
    c += 4;
  }
  return {groups, size};
}

TEST_P(SampleTest, basics)
{
  absl::Time timestamp;
  std::string spec;

  std::tie(timestamp, spec) = GetParam();

  std::vector< metrics::SampleGroup< double > > groups;
  size_t size;
  std::tie(groups, size) = generate(spec, recordBuffer);

  // Create the sample.
  metrics::Sample sample;
  sample.sampleTime(timestamp);
  for(size_t j = 0; j < groups.size(); ++j)
  {
    sample.pushGroup(groups[j]);
  }

  // Test the sample.
  ASSERT_EQ(timestamp, sample.sampleTime());
  ASSERT_EQ(groups.size(), sample.groupCount());
  ASSERT_EQ(size, sample.recordCount());
  for(size_t j = 0; j < sample.groupCount(); ++j)
  {
    ASSERT_EQ(groups[j],
              absl::get< metrics::SampleGroup< double > >(sample.group(j)));
  }
}

TEST_P(SampleTest, append)
{
  absl::Time timestamp;
  std::string spec;

  std::tie(timestamp, spec) = GetParam();

  std::vector< metrics::SampleGroup< double > > groups;
  size_t size;
  std::tie(groups, size) = generate(spec, recordBuffer);

  // Create the sample.
  metrics::Sample sample;
  sample.sampleTime(timestamp);

  std::for_each(groups.begin(), groups.end(), [&](const auto &group) {
    sample.pushGroup(group.records(), group.samplePeriod());
  });

  // Test the sample.
  ASSERT_EQ(timestamp, sample.sampleTime());
  ASSERT_EQ(groups.size(), sample.groupCount());
  ASSERT_EQ(size, sample.recordCount());

  for(size_t j = 0; j < sample.groupCount(); ++j)
  {
    ASSERT_EQ(groups[j],
              absl::get< metrics::SampleGroup< double > >(sample.group(j)));
  }
}

absl::Time
fromYYMMDD(int year, int month, int day)
{
  return absl::FromCivil(absl::CivilDay(year, month, day), absl::UTCTimeZone());
}

std::pair< absl::Time, std::string > sampleTestData[] = {
    {fromYYMMDD(1900, 1, 1), ""},
    {fromYYMMDD(1999, 1, 1), "R1E1"},
    {fromYYMMDD(1999, 2, 1), "R2E2"},
    {fromYYMMDD(2001, 9, 9), "R1E1R2E2"},
    {fromYYMMDD(2001, 9, 9), "R3E3R3E3"},
    {fromYYMMDD(2009, 9, 9), "R2E4R1E1"},
    {fromYYMMDD(2001, 9, 9), "R1E1R2E2R3E3"},
    {fromYYMMDD(2001, 9, 9), "R4E1R3E2R2E3R1E4"},
    {fromYYMMDD(2001, 9, 9), "R1E1R2E2R1E1R2E2R1E1R2E1R1E2"}};

INSTANTIATE_TEST_SUITE_P(MetricsTypes, SampleTest,
                         ::testing::ValuesIn(sampleTestData));
