#include "null_crypto.hpp"
#include <llarp/util/logging/logger.hpp>

#include <limits>

namespace llarp::quic
{
  // Cranks a value to "11", i.e. set it to its maximum
  template <typename T>
  void
  crank_to_eleven(T& val)
  {
    val = std::numeric_limits<T>::max();
  }

  NullCrypto::NullCrypto()
  {
    crank_to_eleven(null_ctx.max_encryption);
    crank_to_eleven(null_ctx.max_decryption_failure);
    null_ctx.aead.max_overhead = 1;  // Fails an assertion if 0
    null_aead.max_overhead = 1;      // FIXME - can this be 0?
  }

  void
  NullCrypto::client_initial(Connection& conn)
  {
    ngtcp2_conn_set_initial_crypto_ctx(conn, &null_ctx);
    ngtcp2_conn_install_initial_key(
        conn,
        &null_aead_ctx,
        null_iv.data(),
        &null_cipher_ctx,
        &null_aead_ctx,
        null_iv.data(),
        &null_cipher_ctx,
        null_iv.size());
    ngtcp2_conn_set_retry_aead(conn, &null_aead, &null_aead_ctx);
    ngtcp2_conn_set_crypto_ctx(conn, &null_ctx);
  }

  void
  NullCrypto::server_initial(Connection& conn)
  {
    LogDebug("Server initial null crypto setup");
    ngtcp2_conn_set_initial_crypto_ctx(conn, &null_ctx);
    ngtcp2_conn_install_initial_key(
        conn,
        &null_aead_ctx,
        null_iv.data(),
        &null_cipher_ctx,
        &null_aead_ctx,
        null_iv.data(),
        &null_cipher_ctx,
        null_iv.size());
    ngtcp2_conn_set_crypto_ctx(conn, &null_ctx);
  }

  bool
  NullCrypto::install_tx_handshake_key(Connection& conn)
  {
    return ngtcp2_conn_install_tx_handshake_key(
               conn, &null_aead_ctx, null_iv.data(), null_iv.size(), &null_cipher_ctx)
        == 0;
  }
  bool
  NullCrypto::install_rx_handshake_key(Connection& conn)
  {
    return ngtcp2_conn_install_rx_handshake_key(
               conn, &null_aead_ctx, null_iv.data(), null_iv.size(), &null_cipher_ctx)
        == 0;
  }
  bool
  NullCrypto::install_tx_key(Connection& conn)
  {
    return ngtcp2_conn_install_tx_key(
               conn,
               null_iv.data(),
               null_iv.size(),
               &null_aead_ctx,
               null_iv.data(),
               null_iv.size(),
               &null_cipher_ctx)
        == 0;
  }
  bool
  NullCrypto::install_rx_key(Connection& conn)
  {
    return ngtcp2_conn_install_rx_key(
               conn, nullptr, 0, &null_aead_ctx, null_iv.data(), null_iv.size(), &null_cipher_ctx)
        == 0;
  }

}  // namespace llarp::quic
