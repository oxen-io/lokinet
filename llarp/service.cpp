#include <llarp/service.hpp>
#include "buffer.hpp"
#include "ini.hpp"
#include "router.hpp"

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

      // write version
      if(!BEncodeWriteDictInt(buf, "v", version))
        return false;
      /*
      if(W)
      {
        if(!BEncodeWriteDictEntry("w", *W, buf))
          return false;
      }
      */
      if(!BEncodeWriteDictEntry("z", Z, buf))
        return false;

      return bencode_end(buf);
    }

    bool
    IntroSet::HasExpiredIntros() const
    {
      auto now = llarp_time_now_ms();
      for(const auto& i : I)
        if(now >= i.expiresAt)
          return true;
      return false;
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
        if(!BEncodeWriteDictInt(buf, "l", latency))
          return false;
      }
      if(!BEncodeWriteDictEntry("p", pathID, buf))
        return false;
      if(!BEncodeWriteDictInt(buf, "v", version))
        return false;
      if(!BEncodeWriteDictInt(buf, "x", expiresAt))
        return false;
      return bencode_end(buf);
    }

    Identity::~Identity()
    {
    }

    bool
    Identity::BEncode(llarp_buffer_t* buf) const
    {
      /// TODO: implement me
      return false;
    }

    bool
    Identity::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
    {
      /// TODO: implement me
      return false;
    }

    void
    Identity::RegenerateKeys(llarp_crypto* crypto)
    {
      crypto->encryption_keygen(enckey);
      crypto->identity_keygen(signkey);
      pub.enckey  = llarp::seckey_topublic(enckey);
      pub.signkey = llarp::seckey_topublic(signkey);
      pub.vanity.Zero();
    }

    bool
    Identity::EnsureKeys(const std::string& fname, llarp_crypto* c)
    {
      // TODO: implement me
      return false;
    }

    bool
    Identity::SignIntroSet(IntroSet& i, llarp_crypto* crypto) const
    {
      if(i.I.size() == 0)
        return false;
      i.A = pub;
      // zero out signature for signing process
      i.Z.Zero();
      byte_t tmp[MAX_INTROSET_SIZE];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(!i.BEncode(&buf))
        return false;
      // rewind and resize buffer
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      return crypto->sign(i.Z, signkey, buf);
    }

    bool
    IntroSet::VerifySignature(llarp_crypto* crypto) const
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
      return crypto->verify(A.signkey, buf, Z);
    }

    bool
    Config::Load(const std::string& fname)
    {
      // TODO: implement me
      ini::Parser parser(fname);
      for(const auto& sec : parser.top().ordered_sections)
      {
        services.push_back({sec->first, sec->second.values});
      }
      return services.size() > 0;
    }

  }  // namespace service
}  // namespace llarp
