#include <util/metrics/stream_publisher.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;
using namespace metrics;

TEST(MetricsPublisher, StreamPublisher)
{
  Category myCategory("MyCategory");
  Description descA(&myCategory, "MetricA");
  Description descB(&myCategory, "MetricB");

  Id metricA(&descA);
  Id metricB(&descB);

  std::stringstream stream;
  StreamPublisher myPublisher(stream);

  std::vector< TaggedRecords< double > > records;

  records.emplace_back(
      metricA,
      TaggedRecordsData< double >{{{}, Record< double >(5, 25.0, 6.0, 25.0)}});
  records.emplace_back(
      metricB,
      TaggedRecordsData< double >{{{}, Record< double >(2, 7.0, 3.0, 11.0)}});

  Sample sample;
  sample.sampleTime(absl::Now());
  sample.pushGroup(records.data(), records.size(), absl::Seconds(5));

  myPublisher.publish(sample);

  std::cout << stream.str();
}
