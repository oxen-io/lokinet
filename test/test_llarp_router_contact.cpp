#include <catch2/catch.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <router_contact.hpp>
#include <net/net_int.hpp>

namespace
{
  llarp::sodium::CryptoLibSodium crypto;
  llarp::CryptoManager cmanager(&crypto);
}

namespace llarp
{

TEST_CASE("RouterContact Sign and Verify", "[RC][RouterContact][signature][sign][verify]")
{
  RouterContact rc;

  SecretKey sign;
  cmanager.instance()->identity_keygen(sign);

  SecretKey encr;
  cmanager.instance()->encryption_keygen(encr);

  rc.enckey = encr.toPublic();
  rc.pubkey = sign.toPublic();

  REQUIRE(rc.Sign(sign));
  REQUIRE(rc.Verify(time_now_ms()));
}

TEST_CASE("RouterContact Decode Version 1", "[RC][RouterContact][V1]")
{
  RouterContact rc;

  SecretKey sign;
  cmanager.instance()->identity_keygen(sign);

  SecretKey encr;
  cmanager.instance()->encryption_keygen(encr);

  rc.version = 1;

  rc.enckey = encr.toPublic();
  rc.pubkey = sign.toPublic();

  REQUIRE(rc.Sign(sign));

  std::array<byte_t, 5000> encoded_buffer;
  llarp_buffer_t encoded_llarp(encoded_buffer);

  rc.BEncode(&encoded_llarp);

  RouterContact decoded_rc;

  REQUIRE(decoded_rc.BDecode(&encoded_llarp));

  REQUIRE(decoded_rc.Verify(time_now_ms()));

  REQUIRE(decoded_rc == rc);
}

} // namespace llarp
