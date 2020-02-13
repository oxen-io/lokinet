#include <dht/txowner.hpp>

#include <gtest/gtest.h>

namespace
{
  using llarp::dht::Key_t;
  using llarp::dht::TXOwner;

  struct TxOwnerData
  {
    Key_t node;
    uint64_t id;
    size_t expectedHash;

    TxOwnerData(const Key_t& k, uint64_t i, size_t h)
        : node(k), id(i), expectedHash(h)
    {
    }
  };

  struct TxOwner : public ::testing::TestWithParam< TxOwnerData >
  {
  };

  TEST_F(TxOwner, default_construct)
  {
    TXOwner dc;
    ASSERT_TRUE(dc.node.IsZero());
    ASSERT_EQ(0u, dc.txid);
    ASSERT_EQ(0u, TXOwner::Hash()(dc));
  }

  TEST_P(TxOwner, hash)
  {
    // test single interactions (constructor and hash)
    auto d = GetParam();
    TXOwner constructor(d.node, d.id);

    ASSERT_EQ(d.expectedHash, TXOwner::Hash()(constructor));
  }

  std::vector< TxOwnerData >
  makeData()
  {
    std::vector< TxOwnerData > result;

    Key_t zero;
    zero.Zero();
    Key_t one;
    one.Fill(0x01);
    Key_t two;
    two.Fill(0x02);

    uint64_t max = std::numeric_limits< uint64_t >::max();

    result.emplace_back(zero, 0, 0ull);
    result.emplace_back(zero, 1, 1ull);
    result.emplace_back(one, 0, 144680345676153346ull);
    result.emplace_back(one, 1, 144680345676153347ull);
    result.emplace_back(two, 0, 289360691352306692ull);
    result.emplace_back(two, 2, 289360691352306694ull);
    result.emplace_back(zero, max, 18446744073709551615ull);
    result.emplace_back(one, max, 18302063728033398269ull);
    result.emplace_back(two, max, 18157383382357244923ull);

    return result;
  }

  struct TxOwnerCmpData
  {
    TXOwner lhs;
    TXOwner rhs;
    bool equal;
    bool less;

    TxOwnerCmpData(const TXOwner& l, const TXOwner& r, bool e, bool ls)
        : lhs(l), rhs(r), equal(e), less(ls)
    {
    }
  };

  struct TxOwnerOps : public ::testing::TestWithParam< TxOwnerCmpData >
  {
  };

  TEST_P(TxOwnerOps, operators)
  {
    // test single interactions (constructor and hash)
    auto d = GetParam();

    ASSERT_EQ(d.lhs == d.rhs, d.equal);
    ASSERT_EQ(d.lhs < d.rhs, d.less);
  }

  std::vector< TxOwnerCmpData >
  makeCmpData()
  {
    std::vector< TxOwnerCmpData > result;

    Key_t zero;
    zero.Fill(0x00);
    Key_t one;
    one.Fill(0x01);
    Key_t two;
    two.Fill(0x02);

    result.emplace_back(TXOwner(zero, 0), TXOwner(zero, 0), true, false);
    result.emplace_back(TXOwner(one, 0), TXOwner(one, 0), true, false);
    result.emplace_back(TXOwner(two, 0), TXOwner(two, 0), true, false);

    result.emplace_back(TXOwner(zero, 0), TXOwner(one, 0), false, true);
    result.emplace_back(TXOwner(two, 0), TXOwner(one, 0), false, false);

    return result;
  }
}  // namespace

INSTANTIATE_TEST_SUITE_P(TestDhtTxOwner, TxOwner,
                        ::testing::ValuesIn(makeData()));

INSTANTIATE_TEST_SUITE_P(TestDhtTxOwner, TxOwnerOps,
                        ::testing::ValuesIn(makeCmpData()));
