#include <service/address.hpp>

#include <gtest/gtest.h>

struct ServiceAddressTest : public ::testing::Test
{
  const std::string snode =
      "8zfiwpgonsu5zpddpxwdurxyb19x6r96xy4qbikff99jwsziws9y.snode";
  const std::string loki =
      "7okic5x5do3uh3usttnqz9ek3uuoemdrwzto1hciwim9f947or6y.loki";
};

TEST_F(ServiceAddressTest, TestParseSNodeNotLoki)
{
  llarp::service::Address addr;
  ASSERT_TRUE(addr.FromString(snode, ".snode"));
  ASSERT_FALSE(addr.FromString(snode, ".loki"));
}

TEST_F(ServiceAddressTest, TestParseLokiNotSNode)
{
  llarp::service::Address addr;
  ASSERT_FALSE(addr.FromString(loki, ".snode"));
  ASSERT_TRUE(addr.FromString(loki, ".loki"));
}
