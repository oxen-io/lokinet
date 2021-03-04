#pragma once

#include <crypto/crypto_libsodium.hpp>
#include <catch2/catch.hpp>

namespace llarp::test
{

  template <typename CryptoImpl = llarp::sodium::CryptoLibSodium>
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
  inline LlarpTest<llarp::sodium::CryptoLibSodium>::~LlarpTest()
  {

  }  // namespace test
}
