#ifndef LLARP_TEST
#define LLARP_TEST

#include <gtest/gtest.h>
#include <crypto/mock_crypto.hpp>

namespace llarp
{
  namespace test
  {
    template < typename CryptoImpl = MockCrypto >
    class LlarpTest : public ::testing::Test
    {
     protected:
      CryptoImpl m_crypto;
      CryptoManager cm;

      LlarpTest() : cm(&m_crypto)
      {
        static_assert(std::is_base_of< Crypto, CryptoImpl >::value, "");
      }
    };
  }  // namespace test
}  // namespace llarp

#endif
