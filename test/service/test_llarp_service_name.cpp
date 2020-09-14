#include "catch2/catch.hpp"
#include <crypto/crypto_libsodium.hpp>
#include <service/address.hpp>
#include <lokimq/hex.h>

using namespace std::literals;

TEST_CASE("Test LNS name decrypt", "[lns]")
{
  llarp::sodium::CryptoLibSodium crypto;
  constexpr auto recordhex = "0ba76cbfdb6dc8f950da57ae781912f31c8ad0c55dbf86b88cb0391f563261a9656571a817be4092969f8a78ee0fcee260424acb4a1f4bbdd27348b71de006b6152dd04ed11bf3c4"sv;
  const auto recordbin = lokimq::from_hex(recordhex);
  CHECK(not recordbin.empty());
  llarp::SymmNonce n{};
  std::vector<byte_t> ciphertext{};
  const auto len = recordbin.size() - n.size();
  std::copy_n(recordbin.cbegin() + len, n.size(), n.data());
  std::copy_n(recordbin.cbegin(), len, std::back_inserter(ciphertext));
  const auto maybe = crypto.maybe_decrypt_name(ciphertext, n, "jason.loki");
  CHECK(maybe.has_value());
  const llarp::service::Address addr{*maybe};
  CHECK(addr.ToString() == "azfoj73snr9f3neh5c6sf7rtbaeabyxhr1m4un5aydsmsrxo964o.loki");
}
