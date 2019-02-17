#include <service/Identity.hpp>

#include <util/fs.hpp>

namespace llarp
{
  namespace service
  {
    Identity::~Identity()
    {
    }

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
        else
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
    Identity::RegenerateKeys(llarp::Crypto* crypto)
    {
      crypto->encryption_keygen(enckey);
      crypto->identity_keygen(signkey);
      pub.Update(llarp::seckey_topublic(enckey),
                 llarp::seckey_topublic(signkey));
      crypto->pqe_keygen(pq);
    }

    bool
    Identity::KeyExchange(path_dh_func dh, SharedSecret& result,
                          const ServiceInfo& other,
                          const KeyExchangeNonce& N) const
    {
      return dh(result, other.EncryptionPublicKey(), enckey, N);
    }

    bool
    Identity::Sign(Crypto* c, Signature& sig, const llarp_buffer_t& buf) const
    {
      return c->sign(sig, signkey, buf);
    }

    bool
    Identity::EnsureKeys(const std::string& fname, llarp::Crypto* c)
    {
      std::array< byte_t, 4096 > tmp;
      llarp_buffer_t buf(tmp);
      std::error_code ec;
      // check for file
      if(!fs::exists(fname, ec))
      {
        if(ec)
        {
          llarp::LogError(ec);
          return false;
        }
        // regen and encode
        RegenerateKeys(c);
        if(!BEncode(&buf))
          return false;
        // rewind
        buf.sz  = buf.cur - buf.base;
        buf.cur = buf.base;
        // write
        std::ofstream f;
        f.open(fname, std::ios::binary);
        if(!f.is_open())
          return false;
        f.write((char*)buf.cur, buf.sz);
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
      if(!BDecode(&buf))
        return false;

      ServiceInfo::OptNonce van;
      if(!vanity.IsZero())
        van = vanity;
      // update pubkeys
      pub.Update(llarp::seckey_topublic(enckey),
                 llarp::seckey_topublic(signkey), van);
      return true;
    }

    bool
    Identity::SignIntroSet(IntroSet& i, llarp::Crypto* crypto,
                           llarp_time_t now) const
    {
      if(i.I.size() == 0)
        return false;
      // set timestamp
      // TODO: round to nearest 1000 ms
      i.T = now;
      // set service info
      i.A = pub;
      // set public encryption key
      i.K = pq_keypair_to_public(pq);
      // zero out signature for signing process
      i.Z.Zero();
      std::array< byte_t, MAX_INTROSET_SIZE > tmp;
      llarp_buffer_t buf(tmp);
      if(!i.BEncode(&buf))
        return false;
      // rewind and resize buffer
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      return Sign(crypto, i.Z, buf);
    }
  }  // namespace service
}  // namespace llarp
