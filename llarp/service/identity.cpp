#include "identity.hpp"

#include <llarp/crypto/crypto.hpp>

namespace llarp::service
{
  bool
  Identity::BEncode(llarp_buffer_t* buf) const
  {
    if (!bencode_start_dict(buf))
      return false;
    if (!BEncodeWriteDictEntry("s", signkey, buf))
      return false;
    if (!BEncodeWriteDictInt("v", version, buf))
      return false;
    return bencode_end(buf);
  }

  bool
  Identity::decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictEntry("s", signkey, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("v", version, read, key, buf))
      return false;
    if (not read)
      return bencode_discard(buf);
    return true;
  }

  void
  Identity::Clear()
  {
    signkey.Zero();
    enckey.Zero();
    pq.Zero();
    derivedSignKey.Zero();
    vanity.Zero();
  }

  void
  Identity::RegenerateKeys()
  {
    crypto::identity_keygen(signkey);
    crypto::encryption_keygen(enckey);
    pub.Update(seckey_to_pubkey(signkey), seckey_to_pubkey(enckey));
    crypto::pqe_keygen(pq);
    if (not crypto::derive_subkey_private(derivedSignKey, signkey, 1))
    {
      throw std::runtime_error("failed to derive subkey");
    }
  }

  bool
  Identity::KeyExchange(
      path_dh_func dh,
      SharedSecret& result,
      const ServiceInfo& other,
      const KeyExchangeNonce& N) const
  {
    return dh(result, other.EncryptionPublicKey(), enckey, N);
  }

  bool
  Identity::Sign(Signature& sig, uint8_t* buf, size_t size) const
  {
    return crypto::sign(sig, signkey, buf, size);
  }

  void
  Identity::EnsureKeys(fs::path fname, bool needBackup)
  {
    // make sure we are empty
    Clear();

    std::array<byte_t, 4096> tmp;

    // this can throw
    bool exists = fs::exists(fname);

    if (exists and needBackup)
    {
      KeyManager::backupFileByMoving(fname);
      exists = false;
    }

    // check for file
    if (!exists)
    {
      llarp_buffer_t buf{tmp};
      // regen and encode
      RegenerateKeys();
      if (!BEncode(&buf))
        throw std::length_error("failed to encode new identity");
      const auto sz = buf.cur - buf.base;
      // write
      try
      {
        util::dump_file(fname, tmp.data(), sz);
      }
      catch (const std::exception& e)
      {
        throw std::runtime_error{fmt::format("failed to write {}: {}", fname, e.what())};
      }
      return;
    }

    if (not fs::is_regular_file(fname))
    {
      throw std::invalid_argument{fmt::format("{} is not a regular file", fname)};
    }

    // read file
    try
    {
      util::file_to_buffer(fname, tmp.data(), tmp.size());
    }
    catch (const std::length_error&)
    {
      throw std::length_error{"service identity too big"};
    }
    // (don't catch io error exceptions)
    {
      llarp_buffer_t buf{tmp};
      if (!bencode_decode_dict(*this, &buf))
        throw std::length_error{"could not decode service identity"};
    }

    // ensure that the encryption key is set
    if (enckey.IsZero())
      crypto::encryption_keygen(enckey);

    // also ensure the ntru key is set
    if (pq.IsZero())
      crypto::pqe_keygen(pq);

    std::optional<VanityNonce> van;
    if (!vanity.IsZero())
      van = vanity;
    // update pubkeys
    pub.Update(seckey_to_pubkey(signkey), seckey_to_pubkey(enckey), van);
    if (not crypto::derive_subkey_private(derivedSignKey, signkey, 1))
    {
      throw std::runtime_error("failed to derive subkey");
    }
  }

  std::optional<EncryptedIntroSet>
  Identity::encrypt_and_sign_introset(const IntroSet& other_i, llarp_time_t now) const
  {
    EncryptedIntroSet encrypted;

    if (other_i.intros.empty())
      return std::nullopt;

    IntroSet i{other_i};
    encrypted.nounce.Randomize();
    // set timestamp
    // TODO: round to nearest 1000 ms
    i.time_signed = now;
    encrypted.signedAt = now;
    // set service info
    i.address_keys = pub;
    // set public encryption key
    i.sntru_pubkey = pq_keypair_to_pubkey(pq);

    auto bte = i.bt_encode();

    const SharedSecret k{i.address_keys.Addr()};
    crypto::xchacha20(reinterpret_cast<uint8_t*>(bte.data()), bte.size(), k, encrypted.nounce);

    std::memcpy(encrypted.introsetPayload.data(), bte.data(), bte.size());

    if (not encrypted.Sign(derivedSignKey))
      return std::nullopt;
    return encrypted;
  }
}  // namespace llarp::service
