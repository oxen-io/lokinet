#ifndef LLARP_TEST
#define LLARP_TEST

#include <crypto/mock_crypto.hpp>
#include <catch2/catch.hpp>
#include <gmock/gmock.h>

namespace llarp
{
  namespace test
  {
    template <typename CryptoImpl = MockCrypto>
    class LlarpTest
    {
     protected:
      CryptoImpl m_crypto;
      CryptoManager cm;

      LlarpTest() : cm(&m_crypto)
      {
        static_assert(std::is_base_of<Crypto, CryptoImpl>::value, "");
      }

      ~LlarpTest()
      {}
    };

    template <>
    inline LlarpTest<MockCrypto>::~LlarpTest()
    {
      CHECK(::testing::Mock::VerifyAndClearExpectations(&m_crypto));
    }
  }  // namespace test
}  // namespace llarp

#endif
