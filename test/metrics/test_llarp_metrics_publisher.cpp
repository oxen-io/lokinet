#include <metrics/publishers.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;

TEST(MetricsPublisher, StreamPublisher)
{
  metrics::Category myCategory("MyCategory");
  metrics::Description descA(&myCategory, "MetricA");
  metrics::Description descB(&myCategory, "MetricB");

  metrics::Id metricA(&descA);
  metrics::Id metricB(&descB);

  std::stringstream stream;
  metrics::StreamPublisher myPublisher(stream);

  std::vector< metrics::Record< double > > records;

  records.emplace_back(metricA, 5, 25.0, 6.0, 25.0);
  records.emplace_back(metricB, 2, 7.0, 3.0, 11.0);

  metrics::Sample< double > sample;
  sample.sampleTime(absl::Now());
  sample.pushGroup(records.data(), records.size(), absl::Seconds(5));

  myPublisher.publish(sample, metrics::Sample< int >());

  std::cout << stream.str();
}
