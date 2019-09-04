#include <util/metrics/metrictank_publisher.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;
using namespace ::testing;

using Interface = metrics::MetricTankPublisherInterface;

TEST(MetricTank, maketags)
{
  Interface::Tags tags;
  std::string result = Interface::makeSuffix(tags);

  ASSERT_THAT(result, Not(IsEmpty()));

  tags["user"] = "Thanos";
  result       = Interface::makeSuffix(tags);
  ASSERT_THAT(result, HasSubstr(";user=Thanos"));
}
