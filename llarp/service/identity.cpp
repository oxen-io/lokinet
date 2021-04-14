#include "identity.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/util/fs.hpp>
#include <sodium/crypto_sign_ed25519.h>

namespace llarp
{
  namespace service
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
    Identity::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
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
      auto crypto = CryptoManager::instance();
      crypto->identity_keygen(signkey);
      crypto->encryption_keygen(enckey);
      pub.Update(seckey_topublic(signkey), seckey_topublic(enckey));
      crypto->pqe_keygen(pq);
      if (not crypto->derive_subkey_private(derivedSignKey, signkey, 1))
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
    Identity::Sign(Signature& sig, const llarp_buffer_t& buf) const
    {
      return CryptoManager::instance()->sign(sig, signkey, buf);
    }

    void
    Identity::EnsureKeys(fs::path fname, bool needBackup)
    {
      // make sure we are empty
      Clear();

      std::array<byte_t, 4096> tmp;
      llarp_buffer_t buf(tmp);

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
        // regen and encode
        RegenerateKeys();
        if (!BEncode(&buf))
          throw std::length_error("failed to encode new identity");
        // rewind
        buf.sz = buf.cur - buf.base;
        buf.cur = buf.base;
        // write
        auto optional_f = util::OpenFileStream<std::ofstream>(fname, std::ios::binary);
        if (!optional_f)
          throw std::runtime_error(stringify("can not open ", fname));
        auto& f = *optional_f;
        if (!f.is_open())
          throw std::runtime_error(stringify("did not open ", fname));
        f.write((char*)buf.cur, buf.sz);
      }

      if (not fs::is_regular_file(fname))
      {
        throw std::invalid_argument(stringify(fname, " is not a regular file"));
      }

      // read file
      std::ifstream inf(fname, std::ios::binary);
      inf.seekg(0, std::ios::end);
      size_t sz = inf.tellg();
      inf.seekg(0, std::ios::beg);

      if (sz > sizeof(tmp))
        throw std::length_error("service identity too big");
      // decode
      inf.read((char*)buf.base, sz);
      if (!bencode_decode_dict(*this, &buf))
        throw std::length_error("could not decode service identity");

      auto crypto = CryptoManager::instance();

      // ensure that the encryption key is set
      if (enckey.IsZero())
        crypto->encryption_keygen(enckey);

      // also ensure the ntru key is set
      if (pq.IsZero())
        crypto->pqe_keygen(pq);

      std::optional<VanityNonce> van;
      if (!vanity.IsZero())
        van = vanity;
      // update pubkeys
      pub.Update(seckey_topublic(signkey), seckey_topublic(enckey), van);
      if (not crypto->derive_subkey_private(derivedSignKey, signkey, 1))
      {
        throw std::runtime_error("failed to derive subkey");
      }
    }

    std::optional<EncryptedIntroSet>
    Identity::EncryptAndSignIntroSet(const IntroSet& other_i, llarp_time_t now) const
    {
      EncryptedIntroSet encrypted;

      if (other_i.intros.empty())
        return std::nullopt;
      IntroSet i{other_i};
      encrypted.nounce.Randomize();
      // set timestamp
      // TODO: round to nearest 1000 ms
      i.timestampSignedAt = now;
      encrypted.signedAt = now;
      // set service info
      i.addressKeys = pub;
      // set public encryption key
      i.sntrupKey = pq_keypair_to_public(pq);
      std::array<byte_t, MAX_INTROSET_SIZE> tmp;
      llarp_buffer_t buf{tmp};
      if (not i.BEncode(&buf))
        return std::nullopt;
      // rewind and resize buffer
      buf.sz = buf.cur - buf.base;
      buf.cur = buf.base;
      const SharedSecret k{i.addressKeys.Addr()};
      CryptoManager::instance()->xchacha20(buf, k, encrypted.nounce);
      encrypted.introsetPayload = buf.copy();

      if (not encrypted.Sign(derivedSignKey))
        return std::nullopt;
      return encrypted;
    }
  }  // namespace service
}  // namespace llarp
