#include <llarp/service.hpp>
#include "buffer.hpp"
#include "fs.hpp"
#include "ini.hpp"

namespace llarp
{
  namespace service
  {
    IntroSet::~IntroSet()
    {
      if(W)
        delete W;
    }

    bool
    IntroSet::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("a", A, read, key, buf))
        return false;

      if(llarp_buffer_eq(key, "i"))
      {
        return BEncodeReadList(I, buf);
      }
      if(!BEncodeMaybeReadDictEntry("k", K, read, key, buf))
        return false;

      if(!BEncodeMaybeReadDictEntry("n", topic, read, key, buf))
        return false;

      if(!BEncodeMaybeReadDictInt("t", T, read, key, buf))
        return false;

      if(llarp_buffer_eq(key, "w"))
      {
        if(W)
          delete W;
        W = new PoW();
        return W->BDecode(buf);
      }

      if(!BEncodeMaybeReadDictInt("v", version, read, key, buf))
        return false;

      if(!BEncodeMaybeReadDictEntry("z", Z, read, key, buf))
        return false;

      return read;
    }

    bool
    IntroSet::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictEntry("a", A, buf))
        return false;
      // start introduction list
      if(!bencode_write_bytestring(buf, "i", 1))
        return false;
      if(!BEncodeWriteList(I.begin(), I.end(), buf))
        return false;
      // end introduction list

      // pq pubkey
      if(!BEncodeWriteDictEntry("k", K, buf))
        return false;

      // topic tag
      if(topic.ToString().size())
      {
        if(!BEncodeWriteDictEntry("n", topic, buf))
          return false;
      }
      // Timestamp published
      if(!BEncodeWriteDictInt("t", T, buf))
        return false;

      // write version
      if(!BEncodeWriteDictInt("v", version, buf))
        return false;
      if(W)
      {
        if(!BEncodeWriteDictEntry("w", *W, buf))
          return false;
      }
      if(!BEncodeWriteDictEntry("z", Z, buf))
        return false;

      return bencode_end(buf);
    }

    bool
    IntroSet::HasExpiredIntros(llarp_time_t now) const
    {
      for(const auto& i : I)
        if(now >= i.expiresAt)
          return true;
      return false;
    }

    bool
    IntroSet::IsExpired(llarp_time_t now) const
    {
      return GetNewestIntroExpiration() < now;
    }

    Introduction::~Introduction()
    {
    }

    bool
    Introduction::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("k", router, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("l", latency, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("p", pathID, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("v", version, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("x", expiresAt, read, key, buf))
        return false;
      return read;
    }

    bool
    Introduction::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;

      if(!BEncodeWriteDictEntry("k", router, buf))
        return false;
      if(latency)
      {
        if(!BEncodeWriteDictInt("l", latency, buf))
          return false;
      }
      if(!BEncodeWriteDictEntry("p", pathID, buf))
        return false;
      if(!BEncodeWriteDictInt("v", version, buf))
        return false;
      if(!BEncodeWriteDictInt("x", expiresAt, buf))
        return false;
      return bencode_end(buf);
    }

    void
    Introduction::Clear()
    {
      router.Zero();
      pathID.Zero();
      latency   = 0;
      expiresAt = 0;
    }

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
    Identity::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("e", enckey, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("q", pq, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("s", signkey, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("v", version, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("x", vanity, read, key, buf))
        return false;
      return read;
    }

    void
    Identity::RegenerateKeys(llarp_crypto* crypto)
    {
      crypto->encryption_keygen(enckey);
      crypto->identity_keygen(signkey);
      pub.Update(llarp::seckey_topublic(enckey),
                 llarp::seckey_topublic(signkey));
      crypto->pqe_keygen(pq);
    }

    bool
    Identity::KeyExchange(llarp_path_dh_func dh, byte_t* result,
                          const ServiceInfo& other, const byte_t* N) const
    {
      return dh(result, other.EncryptionPublicKey(), enckey, N);
    }

    bool
    Identity::Sign(llarp_crypto* c, byte_t* sig, llarp_buffer_t buf) const
    {
      return c->sign(sig, signkey, buf);
    }

    bool
    Identity::EnsureKeys(const std::string& fname, llarp_crypto* c)
    {
      byte_t tmp[4096];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
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

      const byte_t* ptr = nullptr;
      if(!vanity.IsZero())
        ptr = vanity.data();
      // update pubkeys
      pub.Update(llarp::seckey_topublic(enckey),
                 llarp::seckey_topublic(signkey), ptr);
      return true;
    }

    bool
    Identity::SignIntroSet(IntroSet& i, llarp_crypto* crypto,
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
      byte_t tmp[MAX_INTROSET_SIZE];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(!i.BEncode(&buf))
        return false;
      // rewind and resize buffer
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      return Sign(crypto, i.Z, buf);
    }

    bool
    IntroSet::Verify(llarp_crypto* crypto, llarp_time_t now) const
    {
      byte_t tmp[MAX_INTROSET_SIZE];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      IntroSet copy;
      copy = *this;
      copy.Z.Zero();
      if(!copy.BEncode(&buf))
        return false;
      // rewind and resize buffer
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      if(!A.Verify(crypto, buf, Z))
        return false;
      // validate PoW
      if(W && !W->IsValid(crypto->shorthash, now))
        return false;
      // valid timestamps
      // add max clock skew
      now += MAX_INTROSET_TIME_DELTA;
      for(const auto& intro : I)
      {
        if(intro.expiresAt > now
           && intro.expiresAt - now > DEFAULT_PATH_LIFETIME)
        {
          if(W && intro.expiresAt - W->extendedLifetime > DEFAULT_PATH_LIFETIME)
            return false;
          else if(W == nullptr)
          {
            llarp::LogWarn("intro has too high expire time");
            return false;
          }
        }
      }
      if(IsExpired(now))
      {
        llarp::LogWarn("introset expired: ", *this);
        return false;
      }
      return true;
    }

    llarp_time_t
    IntroSet::GetNewestIntroExpiration() const
    {
      llarp_time_t t = 0;
      for(const auto& intro : I)
        t = std::max(intro.expiresAt, t);
      return t;
    }

    bool
    Config::Load(const std::string& fname)
    {
      ini::Parser parser(fname);
      for(const auto& sec : parser.top().ordered_sections)
      {
        services.push_back({sec->first, sec->second.values});
      }
      return services.size() > 0;
    }

  }  // namespace service
}  // namespace llarp
