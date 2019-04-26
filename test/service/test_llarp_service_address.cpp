#include <service/address.hpp>

#include <gtest/gtest.h>

struct ServiceAddressTest : public ::testing::Test
{
  const std::string snode =
      "8zfiwpgonsu5zpddpxwdurxyb19x6r96xy4qbikff99jwsziws9y.snode";
  const std::string loki =
      "7okic5x5do3uh3usttnqz9ek3uuoemdrwzto1hciwim9f947or6y.loki";
  const std::string sub = "lokinet.test";
  const std::string invalid =
      "7okic5x5do3uh3usttnqz9ek3uuoemdrwzto1hciwim9f947or6y.net";
};

TEST_F(ServiceAddressTest, TestParseBadTLD)
{
  llarp::service::Address addr;
  ASSERT_FALSE(addr.FromString(snode, ".net"));
  ASSERT_FALSE(addr.FromString(invalid, ".net"));
}

TEST_F(ServiceAddressTest, TestParseBadTLDAppenedOnEnd)
{
  llarp::service::Address addr;
  const std::string bad = loki + ".net";
  ASSERT_FALSE(addr.FromString(bad, ".net"));
}

TEST_F(ServiceAddressTest, TestParseBadTLDAppenedOnEndWithSubdomain)
{
  llarp::service::Address addr;
  const std::string bad = sub + "." + loki + ".net";
  ASSERT_FALSE(addr.FromString(bad, ".net"));
}

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

TEST_F(ServiceAddressTest, TestParseLokiWithSubdomain)
{
  llarp::service::Address addr;
  const std::string addr_str = sub + "." + loki;
  ASSERT_TRUE(addr.FromString(addr_str, ".loki"));
  ASSERT_EQ(addr.subdomain, sub);
  ASSERT_EQ(addr.ToString(), addr_str);
};

TEST_F(ServiceAddressTest, TestParseSnodeWithSubdomain)
{
  llarp::service::Address addr;
  const std::string addr_str = sub + "." + snode;
  ASSERT_TRUE(addr.FromString(addr_str, ".snode"));
  ASSERT_EQ(addr.subdomain, sub);
  ASSERT_EQ(addr.ToString(".snode"), addr_str);
};
