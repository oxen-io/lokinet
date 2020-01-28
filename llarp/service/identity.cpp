#include <service/identity.hpp>

#include <crypto/crypto.hpp>
#include <util/fs.hpp>
#include <sodium/crypto_sign_ed25519.h>

namespace llarp
{
  namespace service
  {
    bool
    Identity::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictEntry("e", enckey, buf))
        return false;
      if(!BEncodeWriteDictEntry("q", pq, buf))
        return false;
      if(!BEncodeWriteDictEntry("s", signkey, buf))
        return false;
      if(!BEncodeWriteDictInt("v", version, buf))
        return false;
      if(!BEncodeWriteDictEntry("x", vanity, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    Identity::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("e", enckey, read, key, buf))
        return false;
      if(key == "q")
      {
        llarp_buffer_t str;
        if(!bencode_read_string(buf, &str))
          return false;
        if(str.sz == 3200 || str.sz == 2818)
        {
          pq = str.base;
          return true;
        }

        return false;
      }
      if(!BEncodeMaybeReadDictEntry("s", signkey, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("v", version, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("x", vanity, read, key, buf))
        return false;
      return read;
    }

    void
    Identity::RegenerateKeys()
    {
      auto crypto = CryptoManager::instance();
      crypto->identity_keygen(signkey);
      crypto_sign_ed25519_sk_to_curve25519(enckey.data(), signkey.data());
      pub.Update(seckey_topublic(signkey));
      crypto->pqe_keygen(pq);
      if(not crypto->derive_subkey_secret(derivedSignKey, signkey, 1))
      {
        LogError("failed to generate derived key");
      }
    }

    bool
    Identity::KeyExchange(path_dh_func dh, SharedSecret& result,
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

    bool
    Identity::EnsureKeys(const std::string& fname, bool needBackup)
    {
      std::array< byte_t, 4096 > tmp;
      llarp_buffer_t buf(tmp);
      std::error_code ec;

      bool exists = fs::exists(fname, ec);
      if(ec)
      {
        LogError("Could not query file status for ", fname, ": ", ec.message());
        return false;
      }

      if(exists and needBackup)
      {
        KeyManager::backupFileByMoving(fname);
        exists = false;
      }

      // check for file
      if(!exists)
      {
        // regen and encode
        RegenerateKeys();
        if(!BEncode(&buf))
          return false;
        // rewind
        buf.sz  = buf.cur - buf.base;
        buf.cur = buf.base;
        // write
        auto optional_f =
            util::OpenFileStream< std::ofstream >(fname, std::ios::binary);
        if(!optional_f)
          return false;
        auto& f = optional_f.value();
        if(!f.is_open())
          return false;
        f.write((char*)buf.cur, buf.sz);
      }

      if(!fs::is_regular_file(fname))
      {
        LogError("keyfile ", fname, " is not a regular file");
        return false;
      }

      // read file
      std::ifstream inf(fname, std::ios::binary);
      inf.seekg(0, std::ios::end);
      size_t sz = inf.tellg();
      inf.seekg(0, std::ios::beg);

      if(sz > sizeof(tmp))
        return false;
      // decode
      inf.read((char*)buf.base, sz);
      if(!bencode_decode_dict(*this, &buf))
        return false;

      ServiceInfo::OptNonce van;
      if(!vanity.IsZero())
        van = vanity;
      // update pubkeys
      pub.Update(seckey_topublic(signkey), van);
      crypto_sign_ed25519_sk_to_curve25519(enckey.data(), signkey.data());
      auto crypto = CryptoManager::instance();
      return crypto->derive_subkey_secret(derivedSignKey, signkey, 1);
    }

    absl::optional< EncryptedIntroSet >
    Identity::EncryptAndSignIntroSet(const IntroSet& other_i,
                                     llarp_time_t now) const
    {
      EncryptedIntroSet encrypted;

      if(other_i.I.size() == 0)
        return {};
      IntroSet i(other_i);
      encrypted.nounce.Randomize();
      // set timestamp
      // TODO: round to nearest 1000 ms
      i.T                = now;
      encrypted.signedAt = now;
      // set service info
      i.A = pub;
      // set public encryption key
      i.K = pq_keypair_to_public(pq);
      std::array< byte_t, MAX_INTROSET_SIZE > tmp;
      llarp_buffer_t buf(tmp);
      if(not i.BEncode(&buf))
        return {};
      // rewind and resize buffer
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      const SharedSecret k(i.A.Addr());
      CryptoManager::instance()->xchacha20(buf, k, encrypted.nounce);
      encrypted.introsetPayload.resize(buf.sz);
      std::copy_n(buf.base, buf.sz, encrypted.introsetPayload.data());
      if(not encrypted.Sign(derivedSignKey))
        return {};
      return encrypted;
    }
  }  // namespace service
}  // namespace llarp
