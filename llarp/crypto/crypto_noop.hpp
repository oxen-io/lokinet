#ifndef LLARP_CRYPTO_NOOP_HPP
#define LLARP_CRYPTO_NOOP_HPP

#include <crypto/crypto.hpp>

#include <atomic>
#include <numeric>

namespace llarp
{
  struct NoOpCrypto final : public Crypto
  {
   private:
    std::atomic< uint64_t > m_value;

    static constexpr byte_t MAX_BYTE = std::numeric_limits< byte_t >::max();

   public:
    NoOpCrypto() : m_value(0)
    {
    }

    ~NoOpCrypto() override = default;

    bool
    xchacha20(const llarp_buffer_t &, const SharedSecret &,
              const TunnelNonce &) override
    {
      return true;
    }

    bool
    xchacha20_alt(const llarp_buffer_t &out, const llarp_buffer_t &in,
                  const SharedSecret &, const byte_t *) override
    {
      if(in.sz > out.sz)
      {
        return false;
      }

      std::copy_n(in.begin(), in.sz, out.begin());
      return true;
    }

    bool
    dh_client(SharedSecret &shared, const PubKey &pk, const SecretKey &,
              const TunnelNonce &) override
    {
      std::copy_n(pk.begin(), pk.size(), shared.begin());
      return true;
    }

    bool
    dh_server(SharedSecret &shared, const PubKey &pk, const SecretKey &,
              const TunnelNonce &) override
    {
      std::copy_n(pk.begin(), pk.size(), shared.begin());
      return true;
    }

    bool
    transport_dh_client(SharedSecret &shared, const PubKey &pk,
                        const SecretKey &, const TunnelNonce &) override
    {
      std::copy_n(pk.begin(), pk.size(), shared.begin());
      return true;
    }

    bool
    transport_dh_server(SharedSecret &shared, const PubKey &pk,
                        const SecretKey &, const TunnelNonce &) override
    {
      std::copy_n(pk.begin(), pk.size(), shared.begin());
      return true;
    }

    bool
    shorthash(ShortHash &out, const llarp_buffer_t &buff) override
    {
      // copy the first 32 bytes of the buffer
      if(buff.sz < out.size())
      {
        std::copy_n(buff.begin(), buff.sz, out.begin());
        std::fill(out.begin() + buff.sz, out.end(), 0);
      }
      else
      {
        std::copy_n(buff.begin(), out.size(), out.begin());
      }
      return true;
    }

    bool
    hmac(byte_t *out, const llarp_buffer_t &buff, const SharedSecret &) override
    {
      if(buff.sz < HMACSIZE)
      {
        std::copy_n(buff.begin(), buff.sz, out);
        std::fill(out + buff.sz, out + (HMACSIZE - buff.sz), 0);
      }
      else
      {
        std::copy_n(buff.begin(), HMACSIZE, out);
      }
      return true;
    }

    bool
    sign(Signature &sig, const SecretKey &, const llarp_buffer_t &) override
    {
      std::fill(sig.begin(), sig.end(), 0);
      return true;
    }

    bool
    sign(Signature &sig, const PrivateKey &, const llarp_buffer_t &) override
    {
      std::fill(sig.begin(), sig.end(), 0);
      return true;
    }

    bool
    verify(const PubKey &, const llarp_buffer_t &, const Signature &) override
    {
      return true;
    }

    bool
    seed_to_secretkey(SecretKey &key, const IdentitySecret &secret) override
    {
      static_assert(SecretKey::SIZE == (2 * IdentitySecret::SIZE), "");
      std::copy(secret.begin(), secret.end(), key.begin());
      std::copy(secret.begin(), secret.end(),
                key.begin() + IdentitySecret::SIZE);

      return true;
    }

    void
    randomize(const llarp_buffer_t &buff) override
    {
      std::iota(buff.begin(), buff.end(), m_value.load() % MAX_BYTE);
      m_value += buff.sz;
    }

    void
    randbytes(byte_t *ptr, size_t sz) override
    {
      std::iota(ptr, ptr + sz, m_value.load() % MAX_BYTE);
      m_value += sz;
    }

    void
    identity_keygen(SecretKey &key) override
    {
      std::iota(key.begin(), key.end(), m_value.load() % MAX_BYTE);
      m_value += key.size();
    }

    void
    encryption_keygen(SecretKey &key) override
    {
      std::iota(key.begin(), key.end(), m_value.load() % MAX_BYTE);
      m_value += key.size();
    }

    void
    pqe_keygen(PQKeyPair &pair) override
    {
      std::iota(pair.begin(), pair.end(), m_value.load() % MAX_BYTE);
      m_value += pair.size();
    }

    bool
    pqe_decrypt(const PQCipherBlock &block, SharedSecret &secret,
                const byte_t *) override
    {
      std::copy_n(block.begin(), SharedSecret::SIZE, secret.begin());
      return true;
    }

    bool
    pqe_encrypt(PQCipherBlock &block, SharedSecret &secret,
                const PQPubKey &) override
    {
      std::copy_n(secret.begin(), SharedSecret::SIZE, block.begin());
      return true;
    }

    bool
    check_identity_privkey(const SecretKey &) override
    {
      return true;
    }
  };
}  // namespace llarp

#endif
