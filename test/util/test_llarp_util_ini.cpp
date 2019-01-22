#include <gtest/gtest.h>

#include <util/ini.hpp>

struct TestINIParser : public ::testing::Test 
{ 
  llarp::ConfigParser parser;

  void TearDown()
  {
    parser.Clear();
  }
};

TEST_F(TestINIParser, TestParseEmpty)
{
  ASSERT_TRUE(parser.LoadString(""));
}

TEST_F(TestINIParser, TestParseOneSection)
{
  llarp::ConfigParser::Section_t sect;
  auto assertVisit = [&sect](const auto & section) -> bool {
    sect = section;
    return true;
  };
  ASSERT_TRUE(parser.LoadString("[test]\nkey=val"));
  ASSERT_TRUE(parser.VisitSection("test", assertVisit));
  auto itr = sect.find("notfound");
  ASSERT_EQ(itr, sect.end());
  itr = sect.find("key");
  ASSERT_NE(itr, sect.end());
  ASSERT_STREQ(itr->second.c_str(), "val");
}

TEST_F(TestINIParser, TestParseSectionDuplicateKeys)
{
  ASSERT_TRUE(parser.LoadString("[test]\nkey1=val1\nkey1=val2"));
  size_t num = 0;
  auto visit =[&num](const auto & section) -> bool {
    num = section.count("key1");
    return true;
  };
  ASSERT_TRUE(parser.VisitSection("test", visit));
  ASSERT_EQ(num, size_t(2));
}

TEST_F(TestINIParser, TestParseInvalid)
{
  ASSERT_FALSE(parser.LoadString("srged5ghe5\nf34wtge5\nw34tgfs4ygsd5yg=4;\n#g4syhgd5\n"));
}